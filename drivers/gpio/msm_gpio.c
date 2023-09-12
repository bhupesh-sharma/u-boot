// SPDX-License-Identifier: GPL-2.0+
/*
 * Qualcomm GPIO driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <asm/global_data.h>
#include <asm/gpio.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

/* Register offsets */
#define GPIO_CONFIG_OFF(no)         ((no) * 0x1000)
#define GPIO_IN_OUT_OFF(no)         ((no) * 0x1000 + 0x4)

/* OE */
#define GPIO_OE_DISABLE  (0x0 << 9)
#define GPIO_OE_ENABLE   (0x1 << 9)
#define GPIO_OE_MASK     (0x1 << 9)

/* GPIO_IN_OUT register shifts. */
#define GPIO_IN          0
#define GPIO_OUT         1

struct msm_gpio_bank {
	phys_addr_t west_base;
	phys_addr_t north_base;
	phys_addr_t east_base;
	phys_addr_t south_base;
};

static int msm_gpio_direction_input(struct udevice *dev, unsigned int gpio)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);
	phys_addr_t base;
	phys_addr_t reg;
	ofnode dp;

	dp = dev_ofnode(dev);

	if (ofnode_device_is_compatible(dp, "qcom,sm6115-pinctrl"))
		base = (gpio == 89) ? priv->west_base : priv->east_base;

	if (ofnode_device_is_compatible(dp, "qcom,sm8250-pinctrl"))
		base = (gpio == 77) ? priv->north_base : priv->east_base;

	reg = base + GPIO_CONFIG_OFF(gpio);

	/* Disable OE bit */
	clrsetbits_le32(reg, GPIO_OE_MASK, GPIO_OE_DISABLE);

	return 0;
}

static int msm_gpio_set_value(struct udevice *dev, unsigned gpio, int value)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);

	value = !!value;
	/* set value */
	writel(value << GPIO_OUT, priv->east_base + GPIO_IN_OUT_OFF(gpio));

	return 0;
}

static int msm_gpio_direction_output(struct udevice *dev, unsigned gpio,
				     int value)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);
	phys_addr_t base;
	phys_addr_t reg;
	ofnode dp;

	dp = dev_ofnode(dev);

	if (ofnode_device_is_compatible(dp, "qcom,sm6115-pinctrl"))
		base = (gpio == 89) ? priv->west_base : priv->east_base;

	if (ofnode_device_is_compatible(dp, "qcom,sm8250-pinctrl"))
		base = (gpio == 77) ? priv->north_base : priv->east_base;

	reg = base + GPIO_CONFIG_OFF(gpio);
	value = !!value;
	/* set value */
	writel(value << GPIO_OUT, base + GPIO_IN_OUT_OFF(gpio));
	/* switch direction */
	clrsetbits_le32(reg, GPIO_OE_MASK, GPIO_OE_ENABLE);

	return 0;
}

static int msm_gpio_get_value(struct udevice *dev, unsigned gpio)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);
	phys_addr_t base = priv->east_base;
	ofnode dp;

	dp = dev_ofnode(dev);

	if (ofnode_device_is_compatible(dp, "qcom,sm8250-pinctrl"))
		base = (gpio == 77) ? priv->north_base : priv->east_base;

	debug("%s: base: 0x%llx, base + GPIO_IN_OUT_OFF(gpio): 0x%llx, gpio: %d\n",
		       __func__, base, base + GPIO_IN_OUT_OFF(gpio), gpio);
	return !!(readl(base + GPIO_IN_OUT_OFF(gpio)) >> GPIO_IN);
}

static int msm_gpio_get_function(struct udevice *dev, unsigned offset)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);
	phys_addr_t base;
	ofnode dp;

	dp = dev_ofnode(dev);

	if (ofnode_device_is_compatible(dp, "qcom,sm6115-pinctrl"))
		base = (offset == 89) ? priv->west_base : priv->east_base;

	if (ofnode_device_is_compatible(dp, "qcom,sm8250-pinctrl"))
		base = (offset == 77) ? priv->north_base : priv->east_base;

	if (readl(base + GPIO_CONFIG_OFF(offset)) & GPIO_OE_ENABLE)
		return GPIOF_OUTPUT;

	return GPIOF_INPUT;
}

static const struct dm_gpio_ops gpio_msm_ops = {
	.direction_input	= msm_gpio_direction_input,
	.direction_output	= msm_gpio_direction_output,
	.get_value		= msm_gpio_get_value,
	.set_value		= msm_gpio_set_value,
	.get_function		= msm_gpio_get_function,
};

static int msm_gpio_probe(struct udevice *dev)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);
	phys_addr_t reg;
	int value;
	ofnode dp;

	dp = dev_ofnode(dev);

	if (ofnode_device_is_compatible(dp, "qcom,sm6115-pinctrl")) {
		priv->west_base = dev_read_addr_index(dev, 0);
		priv->south_base = dev_read_addr_index(dev, 1);
		priv->east_base = dev_read_addr_index(dev, 2);

		priv->west_base = 0x00500000;
		priv->south_base = 0x00900000;
		priv->east_base = 0x00d00000;

		if (priv->east_base == FDT_ADDR_T_NONE) {
			printf("%s: Error:: failed getting base_addr\n", __func__);
			return -EINVAL;
		}

		printf("priv->west_base (0x%llx)\n", priv->west_base);
		printf("priv->south_base (0x%llx)\n", priv->south_base);
		printf("priv->east_base (0x%llx)\n", priv->east_base);

		value = 1;
		// west
		reg = priv->west_base + GPIO_CONFIG_OFF(89);
		value = !!value;
		/* set value */
		writel(value << GPIO_OUT, priv->west_base + GPIO_IN_OUT_OFF(89));
		/* switch direction */
		clrsetbits_le32(reg, GPIO_OE_MASK, GPIO_OE_ENABLE);

		value = 1;
		// east
		reg = priv->east_base + GPIO_CONFIG_OFF(37);
		value = !!value;
		/* set value */
		writel(value << GPIO_OUT, priv->east_base + GPIO_IN_OUT_OFF(37));
		/* switch direction */
		clrsetbits_le32(reg, GPIO_OE_MASK, GPIO_OE_ENABLE);
	}

	if (ofnode_device_is_compatible(dp, "qcom,sm8250-pinctrl")) {
		priv->west_base = dev_read_addr_index(dev, 0);
		priv->south_base = dev_read_addr_index(dev, 1);
		priv->north_base = dev_read_addr_index(dev, 2);

		priv->west_base = 0x0f100000;
		priv->south_base = 0x0f500000;
		priv->north_base = 0x0f900000;

		if (priv->north_base == FDT_ADDR_T_NONE) {
			printf("%s: Error:: failed getting base_addr\n", __func__);
			return -EINVAL;
		}

		printf("priv->west_base (0x%llx)\n", priv->west_base);
		printf("priv->south_base (0x%llx)\n", priv->south_base);
		printf("priv->north_base (0x%llx)\n", priv->north_base);
	}

	if (priv->west_base == FDT_ADDR_T_NONE ||
	    priv->south_base == FDT_ADDR_T_NONE) {
		printf("%s: Error:: failed getting base_addr\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int msm_gpio_of_to_plat(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	uc_priv->gpio_count = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
					     "gpio-count", 0);
	uc_priv->bank_name = fdt_getprop(gd->fdt_blob, dev_of_offset(dev),
					 "gpio-bank-name", NULL);
	if (uc_priv->bank_name == NULL)
		uc_priv->bank_name = "soc";

	return 0;
}

U_BOOT_DRIVER(gpio_msm) = {
	.name	= "gpio_msm",
	.id	= UCLASS_GPIO,
	.of_to_plat = msm_gpio_of_to_plat,
	.probe	= msm_gpio_probe,
	.ops	= &gpio_msm_ops,
	.flags	= DM_UC_FLAG_SEQ_ALIAS,
	.priv_auto	= sizeof(struct msm_gpio_bank),
};
