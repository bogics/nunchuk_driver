/* Download it from http://j.mp/GQ2yuD */
// - https://lwn.net/Articles/443043/
// - https://www.kernel.org/doc/Documentation/input/input-programming.txt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input-polldev.h>
#include <linux/string.h>

struct nunchuk_dev {
	struct input_polled_dev *polled_input; 
	struct i2c_client *i2c_client;
	};


static int nunchuk_read_registers(struct i2c_client *client,
				   u8 *recv)
{
	u8 buf[1];
	int ret;

	/* Ask the device to get ready for a read */

	mdelay(10);

	buf[0] = 0x00;
	ret = i2c_master_send(client, buf, 1);
	if (ret != 1) {
		dev_err(&client->dev, "i2c send failed (%d)\n", ret);
		return ret < 0 ? ret : -EIO;
	}

	mdelay(10);

	/* Now read registers */

	ret = i2c_master_recv(client, recv, 6);
	if (ret != 6) {
		dev_err(&client->dev, "i2c recv failed (%d)\n", ret);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

static void nunchuck_poll(struct input_polled_dev *polled_input)
{
    u8 recv[6];
    int joyX, joyY, AccX, AccY, AccZ;
    int ret, zpressed, cpressed;
	struct nunchuk_dev *nunchukDev = polled_input->private;
    struct i2c_client *client = nunchukDev->i2c_client;

	if ((ret = nunchuk_read_registers(client, recv)) < 0)
		return;

	AccX = (recv[2] << 2) & ((recv[5] >> 2) & 0x3);
	AccY = (recv[3] << 2) & ((recv[5] >> 4) & 0x3);
	AccZ = (recv[4] << 2) & ((recv[5] >> 6) & 0x3);

    joyX = recv[0];
    joyY = recv[1];
    
    zpressed = (recv[5] & BIT(0)) ? 0 : 1;
    //pr_info("\nJoystick position: %d:%d", joyX, joyY);
	if (zpressed)
		dev_info(&client->dev, "Z button pressed\n");

	cpressed = (recv[5] & BIT(1)) ? 0 : 1;

	if (cpressed)
		dev_info(&client->dev, "C button pressed\n");

    
    //input_event(polled_input->input, EV_KEY, BTN_Z, zpressed);
    input_report_key(polled_input->input, BTN_Z, zpressed);
    input_event(polled_input->input, EV_KEY, BTN_C, cpressed);
    
		input_report_abs(polled_input->input, ABS_X, joyX);
		input_report_abs(polled_input->input, ABS_Y, joyY);
		input_report_abs(polled_input->input, ABS_RX, AccX);
		input_report_abs(polled_input->input, ABS_RY, AccY);
		input_report_abs(polled_input->input, ABS_RZ, AccZ);
    input_sync(polled_input->input);
}


static int nunchuk_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	u8 buf[2];
    int ret, err;
	struct input_polled_dev *polled_input;
	struct input_dev *input;
	struct nunchuk_dev *nunchukDev;
	
	pr_info("Called nunchuk_probe, id->name: %s\n", id->name);

	nunchukDev = devm_kzalloc(&client->dev, sizeof(struct nunchuk_dev), GFP_KERNEL);
	if (!nunchukDev) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	
	/* Allocate memory for polled device */
	polled_input = input_allocate_polled_device();
	if (!polled_input)
		return -ENOMEM;

	nunchukDev->i2c_client = client;
	nunchukDev->polled_input = polled_input;
	polled_input->private = nunchukDev;
    polled_input->poll = nunchuck_poll;
    polled_input->poll_interval = 50;
	i2c_set_clientdata(client, nunchukDev);
	input = polled_input->input;
	input->dev.parent = &client->dev;
	
	input->name = "Wii Nunchuk";
	input->id.bustype = BUS_I2C;

	/* Declare the events generated by this driver */
	set_bit(EV_ABS, input->evbit);
	set_bit(ABS_X, input->absbit); /* joystick */
	set_bit(ABS_Y, input->absbit);
	set_bit(ABS_RX, input->absbit); /* accelerometer */
	set_bit(ABS_RY, input->absbit);
	set_bit(ABS_RZ, input->absbit);
    set_bit(EV_KEY, input->evbit);
	set_bit(BTN_C, input->keybit);
	set_bit(BTN_Z, input->keybit);

	input_set_abs_params(input, ABS_X, 30, 220, 4, 8);
	input_set_abs_params(input, ABS_Y, 40, 200, 4, 8);
	input_set_abs_params(input, ABS_RX, 0, 0x3ff, 4, 8);
	input_set_abs_params(input, ABS_RY, 0, 0x3ff, 4, 8);
	input_set_abs_params(input, ABS_RZ, 0, 0x3ff, 4, 8);

	/* Register polled device */
	err = input_register_polled_device(polled_input);
	if (err)
		goto out_err;
		
		
	/* Initialize device */

	//buf[0] = 0x40; 
	buf[0] = 0xf0;
	//buf[1] = 0x00; 
	buf[1] = 0x55;

	ret = i2c_master_send(client, buf, 2);
	if (ret != 2) {
		dev_err(&client->dev, "i2c send failed (%d)\n", ret);
		return ret < 0 ? ret : -EIO;
	}

	udelay(1000);

	//buf[0] = 0x00;//0xfb;
	buf[0] = 0xfb;
	buf[1] = 0x00;

	pr_info("nunchuk_probe OK\n");
	return 0;

out_err:
	input_free_polled_device(polled_input);
	return err;
}

static int nunchuk_remove(struct i2c_client *client)
{
	struct nunchuk_dev *nunchukDev;
	pr_info("Called nunchuk_remove\n");
	
	nunchukDev = i2c_get_clientdata(client);
	input_unregister_polled_device(nunchukDev->polled_input);
	input_free_polled_device(nunchukDev->polled_input);
	
	return 0;
}

/* for non-DT based probing of i2c devices */
static const struct i2c_device_id nunchuk_id[] = {
	{ "nunchuk", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, nunchuk_id);

#ifdef CONFIG_OF
static struct of_device_id nunchuk_dt_match[] = {
	{ .compatible = "nintendo,nunchuk" },
	{ },
};
MODULE_DEVICE_TABLE(of, nunchuk_dt_match); 
#endif

/* struct i2c_driver that inherits from struct device_driver */
static struct i2c_driver nunchuk_driver = {
	.driver = {
		.name = "nunchuk",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(nunchuk_dt_match),
	},
	.probe = nunchuk_probe,
	.remove = nunchuk_remove,
	.id_table = nunchuk_id,
};

/* Register i2c device driver
 * module_i2c_driver() -> i2c_register_driver() -> driver_register()
 * */
module_i2c_driver(nunchuk_driver);

MODULE_LICENSE("GPL");
