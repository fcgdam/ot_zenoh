#include <zephyr.h>
#include <net/openthread.h>
#include <openthread/thread.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <string.h>
#include <usb/usb_device.h>
#include <drivers/uart.h>

#include <zenoh-pico.h>

#include <stdio.h>
#include <stdlib.h>

#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
	     "Console device is not ACM CDC UART device");

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED1_NODE DT_ALIAS(led1)

#if DT_NODE_HAS_STATUS(LED1_NODE, okay)
#define LED1	DT_GPIO_LABEL(LED1_NODE, gpios)
#define PIN	DT_GPIO_PIN(LED1_NODE, gpios)
#define FLAGS	DT_GPIO_FLAGS(LED1_NODE, gpios)
#else
/* A build error here means your board isn't set up to blink an LED. */
#error "Unsupported board: led0 devicetree alias is not defined"
#define LED1	""
#define PIN	0
#define FLAGS	0
#endif

#define LED0_NODE DT_ALIAS(led0)

volatile int otIsConnected = 0;
int gled_rate = 750;

struct led {
    const    char *gpio_dev_name;
    const    char *gpio_pin_name;
    unsigned int  gpio_pin;
    unsigned int  gpio_flags;
};

/*
 * Blinks a specific LED. This code is reuseable if we want to blink multiple LEDs
*/

void blink(const struct led *led, uint32_t id) {
    const struct device *gpio_dev;
    int ret;
    bool led_is_on = true;

    gpio_dev = device_get_binding(led->gpio_dev_name);
    if (gpio_dev == NULL) {
        printk("Error: didn't find %s device\n",
        led->gpio_dev_name);
        return;
    }

    ret = gpio_pin_configure(gpio_dev, led->gpio_pin, led->gpio_flags);
    if (ret != 0) {
        printk("Error %d: failed to configure pin %d '%s'\n",
        ret, led->gpio_pin, led->gpio_pin_name);
    return;
    }

    // Blink for ever.
	while (1) {
		gpio_pin_set(gpio_dev, led->gpio_pin, (int)led_is_on);
		led_is_on = !led_is_on;
		k_msleep(gled_rate);    // We should sleep, otherwise the task won't release the cpu for other tasks!
	}
}

/*
 * Task for blinking led0, which is the blue pill onboard only led.
*/

void blink0(void) {
    const struct led led0 = {
    #if DT_NODE_HAS_STATUS(LED0_NODE, okay)
        .gpio_dev_name = DT_GPIO_LABEL(LED0_NODE, gpios),
        .gpio_pin_name = DT_LABEL(LED0_NODE),
        .gpio_pin = DT_GPIO_PIN(LED0_NODE, gpios),
        .gpio_flags = GPIO_OUTPUT | DT_GPIO_FLAGS(LED0_NODE, gpios),
    #else
        #error "Unsupported board: led0 devicetree alias is not defined"
    #endif
    };

    blink(&led0, 0);
}

void handleNetifStateChanged(uint32_t aFlags, void *aContext)
{
    printk("Openthread: Device Role changed.\n");

    struct openthread_context *ot_context = aContext;

    if ((aFlags & OT_CHANGED_THREAD_ROLE) != 0) {
        otDeviceRole changedRole = otThreadGetDeviceRole(ot_context->instance);

        switch (changedRole) {
            case OT_DEVICE_ROLE_LEADER:
                otIsConnected = 1;
                gled_rate = 500;
            break;

            case OT_DEVICE_ROLE_ROUTER:
                otIsConnected = 1;
                gled_rate = 500;
            break;

            case OT_DEVICE_ROLE_CHILD:
                otIsConnected = 1;
                gled_rate = 500;
            break;

           case OT_DEVICE_ROLE_DETACHED:
           case OT_DEVICE_ROLE_DISABLED:
                otIsConnected = 0;
                gled_rate = 100;
           break;
        }
    }
}


void data_handler(const zn_sample_t *sample, const void *arg)
{
    const struct device *dev = (struct device*)arg;
    static bool led_is_on = false;

    gpio_pin_set(dev, PIN, (int)led_is_on);
    led_is_on = !led_is_on;

    printk(">> [Subscription listener] Received (%.*s, %.*s)\n",
           (int)sample->key.len, sample->key.val,
           (int)sample->value.len, sample->value.val);
}

void usb_console_init(void) {
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    uint32_t dtr = 0;

    if (usb_enable(NULL)) {
        return;
    }

    /* Poll if the DTR flag was set, optional */
    while (!dtr) {
        uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
        k_msleep(250);          // Let other tasks to run if no terminal is connected to USB
    }

    while ( 1 ) {
        //printk("Hello from CDC Virtual COM port!\n\n");
        k_msleep( 2000 );
    }
}


void zenoh_task(void)
{
    const struct device *dev;
	bool led_is_on = false;
    int ret;

    k_msleep(2500);   // Wait for us to have time to connect to the console.


    struct openthread_context *otContext = openthread_get_default_context();
    otInstance *ot_instance = openthread_get_default_instance();

    //otSetStateChangedCallback( ot_instance , handleNetifStateChanged, ot_instance );
    openthread_set_state_changed_cb( handleNetifStateChanged );


    dev = device_get_binding(LED1);
	if (dev == NULL) {
		return;
	}

	ret = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
	if (ret < 0) {
		return;
	}

    while ( otIsConnected == 0 ) {
        printk("Waiting for connection to Openthread network...\n");
        k_msleep( 1000 );
        if ( ( otThreadGetDeviceRole(otContext->instance) != OT_DEVICE_ROLE_DISABLED) && ( otThreadGetDeviceRole(otContext->instance) != OT_DEVICE_ROLE_DETACHED) ) {
            printk("Connected.\n");
            otIsConnected = 1;
        }
    }

    printk("Connected to openthread network.\n");


    // Set initial state to off
    gpio_pin_set(dev, PIN, (int)led_is_on);

    char *uri = "/demo/example/**";

    k_msleep( 2500 );

    zn_properties_t *config = zn_config_default();
    zn_properties_insert(config, ZN_CONFIG_PEER_KEY, z_string_make( "tcp/[fd11:1111:1122:2222:c2d8:7ba9:9e6d:5d3f]:7447" ));

    printk("Zenoh Session init...!\n");
    zn_session_t *s = NULL;
    while( s == NULL )
    {
        printk( "Connecting!\n" );
        s = zn_open(config);
        if (s == 0)
        {
            printk("Unable to open session!\n");

            k_msleep( 3000 );
        }
    }
    
    // Start the read session session lease loops
    int rez = znp_start_read_task(s);
    if( rez == 0 )
    {
        printk( "Started read task\n" );
    }
    else{
        printk( "Failed read task %d\n", rez );
    }
    znp_start_lease_task(s);

    zn_subscriber_t *sub = zn_declare_subscriber(s, zn_rname(uri), zn_subinfo_default(), data_handler, dev );
    if (sub == 0)
    {
        printk("Unable to declare subscriber.\n");
        
    }

    printk("Awaiting data\n"); 
    while( true )
    {
        printk("waiting....\n");
        k_msleep( 1000 );
    }

    zn_undeclare_subscriber(sub);
    zn_close(s);

    return;    
}


K_THREAD_DEFINE(zenoh_id, STACKSIZE*6, zenoh_task, NULL, NULL, NULL, PRIORITY, 0, 0);

K_THREAD_DEFINE(console_id, STACKSIZE, usb_console_init, NULL, NULL, NULL, PRIORITY, 0, 0);

K_THREAD_DEFINE(blink0_id, STACKSIZE, blink0, NULL, NULL, NULL, PRIORITY, 0, 0); 
