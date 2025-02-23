#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

#include <zephyr/display/mb_display.h>

#define SCROLL_SPEED       800  /* Text scrolling speed */

#define LED_MAX            4    /* LEDs maximum x and y position */

#define START_SNAKE_LENGTH 2    /* Initial snake length */
#define X_START            2    /* Snake starting x position */
#define MOVEMENT_DELAY     500  /* Time between each snake movement */

// Define buttons A (0) and B (1).
static const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET_OR(
                                    DT_ALIAS(sw0), gpios, {0});
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET_OR(
                                    DT_ALIAS(sw1), gpios, {0});

static struct gpio_callback button_cb_data;

struct x_y {
    int x;
    int y;
};

enum directions {
    UP,
    RIGHT,
    DOWN,
    LEFT
};

// The direction the snake head is going in.
static enum directions snakehead_direction = UP;

static int score = 0;

// Snake head position.
static struct x_y head_position = {X_START, LED_MAX-START_SNAKE_LENGTH+1};

// Snake food item position.
static struct x_y food_position;

void
generate_food_position(void)
{
    /* Continuously generate random x-y coordinates for the food item
    *  until the position is different from the snake's head position.
    */
    do {
        food_position.x = sys_rand16_get() % LED_MAX + 1;
        food_position.y = sys_rand16_get() % LED_MAX + 1;
    } while(food_position.x == head_position.x &&
             food_position.y == head_position.y);
}

static void
button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	if (pins & BIT(button0.pin)) {
		printk("A pressed\n");
        snakehead_direction = (snakehead_direction + LEFT) % 4;
	}
    else {
		printk("B pressed\n");
        snakehead_direction = (snakehead_direction + RIGHT) % 4;
	}
}

static void
configure_buttons(void)
{
    int ret;

    // Check if button port is ready, button0.port == button1.port.
    if (!gpio_is_ready_dt(&button0)) {
        printk("Error: button device %s is not ready\n",
            button0.port->name);
        return;
    }

    // Configure button0 as input.
    ret = gpio_pin_configure_dt(&button0, GPIO_INPUT);
    if (ret != 0) {
        printk("Error %d: failed to configure %s pin %d\n",
            ret, button0.port->name, button0.pin);
        return;
    }

    // Configure button1 as input.
    ret = gpio_pin_configure_dt(&button1, GPIO_INPUT);
    if (ret != 0) {
        printk("Error %d: failed to configure %s pin %d\n",
            ret, button1.port->name, button1.pin);
        return;
    }

    // Configure interrupt for button0.
    ret = gpio_pin_interrupt_configure_dt(&button0,
             GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        printk("Error %d: failed to configure interrupt on %s pin %d\n",
            ret, button0.port->name, button0.pin);
        return;
    }

    // Configure interrupt for button1.
    ret = gpio_pin_interrupt_configure_dt(&button1,
             GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        printk("Error %d: failed to configure interrupt on %s pin %d\n",
            ret, button1.port->name, button1.pin);
        return;
    }

    // Add callback function button_pressed to run when pressing either button.
    gpio_init_callback(&button_cb_data, button_pressed,
                  BIT(button0.pin) | BIT(button1.pin));
    gpio_add_callback(button0.port, &button_cb_data);
}

void
game_end(void)
{
    struct mb_display *disp = mb_display_get();

    /* Continuously display the score as a scrolling image if it is at least
    *  a two-digit number. Otherwise, display it as a single image.
    */
    if (score >= 10) {
        mb_display_print(disp, MB_DISPLAY_MODE_DEFAULT | MB_DISPLAY_FLAG_LOOP,
                 SCROLL_SPEED, "%d", score);
    }
    else {
        mb_display_print(disp, MB_DISPLAY_MODE_SINGLE, SYS_FOREVER_MS,
                 "%d", score);
    }
}

int
main(void)
{
    struct mb_display *disp = mb_display_get();

    configure_buttons();

    // Generate initial item position.
    generate_food_position();

    while (1) {
        struct mb_image img = {};

        // Generate the LED image with the snake and food item LED positions.
        if (head_position.y == food_position.y) {
            img.row[head_position.y] = (BIT(head_position.x) | BIT(food_position.x));
        }
        else {
            img.row[food_position.y] = BIT(food_position.x);
            img.row[head_position.y] = BIT(head_position.x);
        }

        mb_display_image(disp, MB_DISPLAY_MODE_DEFAULT, SYS_FOREVER_MS, &img, 1);

        k_msleep(MOVEMENT_DELAY);

        switch(snakehead_direction) {
            case UP:
                head_position.y--;
                break;
            case RIGHT:
                head_position.x++;
                break;
            case DOWN:
                head_position.y++;
                break;
            case LEFT:
                head_position.x--;
                break;
        }

        // Game ends if snake head position is out of bounds.
        if (head_position.x < 0 || head_position.x > LED_MAX ||
              head_position.y < 0 || head_position.y > LED_MAX) {
            break;
        }

        // Food is eaten if true.
        if (head_position.x == food_position.x &&
              head_position.y == food_position.y) {
            score++;
            generate_food_position();
        }
    }

    game_end();
    return 0;
}