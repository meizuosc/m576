
/* To support PBA function test */
static struct class *jack_class;
static struct device *jack_dev;

static ssize_t earjack_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct arizona_extcon_info *info = dev_get_drvdata(dev);
	int status = info->edev.state;
	int report = 0;

	if ((status & BIT_HEADSET) ||
		(status & BIT_HEADSET_NO_MIC)) {
		report = 1;
	}

	return sprintf(buf, "%d\n", report);
}

static ssize_t earjack_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_key_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct arizona_extcon_info *info = dev_get_drvdata(dev);
	struct arizona *arizona = info->arizona;
	unsigned int val, lvl;
	int report = 0;
	int ret, key;


	ret = regmap_read(arizona->regmap, ARIZONA_MIC_DETECT_3, &val);
	if (ret != 0)
		dev_err(arizona->dev, "Failed to read MICDET: %d\n", ret);

	dev_err(arizona->dev, "MICDET: %x\n", val);

	if (val & MICD_LVL_0_TO_7) {
		if (info->mic) {
			lvl = val & ARIZONA_MICD_LVL_MASK;
			lvl >>= ARIZONA_MICD_LVL_SHIFT;

			WARN_ON(!lvl);
			WARN_ON(ffs(lvl) - 1 >= info->num_micd_ranges);
			if (lvl && ffs(lvl) - 1 < info->num_micd_ranges) {
				key = info->micd_ranges[ffs(lvl) - 1].key;

				if (key == KEY_MEDIA)
					report = true;
			}
		}
	}

	return sprintf(buf, "%d\n", report);
}

static ssize_t earjack_key_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_select_jack_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);

	return 0;
}

static ssize_t earjack_select_jack_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct arizona_extcon_info *info = dev_get_drvdata(dev);

	if ((!size) || (buf[0] != '1')) {
		switch_set_state(&info->edev, 0);
		pr_info("Forced remove microphone\n");
	} else {
		switch_set_state(&info->edev, 1);
		pr_info("Forced detect microphone\n");
	}

	return size;
}

static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_select_jack_show, earjack_select_jack_store);

static DEVICE_ATTR(key_state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_key_state_show, earjack_key_state_store);

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_state_show, earjack_state_store);


static void create_jack_devices(struct arizona_extcon_info *info)
{
	/* To support PBA function test */
	jack_class = class_create(THIS_MODULE, "audio");

	if (IS_ERR(jack_class))
		pr_err("Failed to create class\n");

	jack_dev = device_create(jack_class, NULL, 0, info, "earjack");

	if (device_create_file(jack_dev, &dev_attr_select_jack) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_select_jack.attr.name);

	if (device_create_file(jack_dev, &dev_attr_key_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_key_state.attr.name);

	if (device_create_file(jack_dev, &dev_attr_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_state.attr.name);
}
