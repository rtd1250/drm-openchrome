/*
 * Copyright 2011 James Simmons <jsimmons@infradead.org> All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation;
 * either version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTIES OR REPRESENTATIONS; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * 
 * This part was influenced by the via i2c code written for the viafb
 * driver by VIA Technologies and S3 Graphics
 */

#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <asm/olpc.h>

#include "drm.h"
#include "drmP.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"

#include "via_drv.h"

/*
 * The default port config.
 */
static struct via_port_cfg adap_configs[] = {
	[VIA_PORT_26]	= { VIA_PORT_I2C,  VIA_MODE_I2C,  VIASR, 0x26 },
	[VIA_PORT_31]	= { VIA_PORT_I2C,  VIA_MODE_I2C,  VIASR, 0x31 },
	[VIA_PORT_25]	= { VIA_PORT_GPIO, VIA_MODE_GPIO, VIASR, 0x25 },
	[VIA_PORT_2C]	= { VIA_PORT_GPIO, VIA_MODE_I2C,  VIASR, 0x2c },
	[VIA_PORT_3D]	= { VIA_PORT_GPIO, VIA_MODE_GPIO, VIASR, 0x3d }
};

static void via_i2c_setscl(void *data, int state)
{
	struct via_i2c *i2c = data;
	struct via_port_cfg *adap_data = i2c->adap_cfg;
	struct drm_via_private *dev_priv = i2c->dev_priv;
	unsigned long flags;
	u8 val;

	spin_lock_irqsave(&dev_priv->mmio_lock, flags);
	val = seq_ioread8(VGABASE, adap_data->ioport_index) & 0xF0;
	if (state)
		val |= 0x20;
	else
		val &= ~0x20;
	switch (adap_data->type) {
	case VIA_PORT_I2C:
		val |= 0x01;
		break;
	case VIA_PORT_GPIO:
		val |= 0x80;
		break;
	default:
		printk(KERN_ERR "via_i2c: specify wrong i2c type.\n");
	}
	seq_iowrite8(VGABASE, adap_data->ioport_index, val);
	spin_unlock_irqrestore(&dev_priv->mmio_lock, flags);
}

static int via_i2c_getscl(void *data)
{
	struct via_i2c *i2c = data;
	struct via_port_cfg *adap_data = i2c->adap_cfg;
	struct drm_via_private *dev_priv = i2c->dev_priv;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dev_priv->mmio_lock, flags);
	if (seq_ioread8(VGABASE, adap_data->ioport_index) & 0x08)
		ret = 1;
	spin_unlock_irqrestore(&dev_priv->mmio_lock, flags);
	return ret;
}

static int via_i2c_getsda(void *data)
{
	struct via_i2c *i2c = data;
	struct via_port_cfg *adap_data = i2c->adap_cfg;
	struct drm_via_private *dev_priv = i2c->dev_priv;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dev_priv->mmio_lock, flags);
	if (seq_ioread8(VGABASE, adap_data->ioport_index) & 0x04)
		ret = 1;
	spin_unlock_irqrestore(&dev_priv->mmio_lock, flags);
	return ret;
}

static void via_i2c_setsda(void *data, int state)
{
	struct via_i2c *i2c = data;
	struct via_port_cfg *adap_data = i2c->adap_cfg;
	struct drm_via_private *dev_priv = i2c->dev_priv;
	unsigned long flags;
	u8 val;

	spin_lock_irqsave(&dev_priv->mmio_lock, flags);
	val = seq_ioread8(VGABASE, adap_data->ioport_index) & 0xF0;
	if (state)
		val |= 0x10;
	else
		val &= ~0x10;
	switch (adap_data->type) {
	case VIA_PORT_I2C:
		val |= 0x01;
		break;
	case VIA_PORT_GPIO:
		val |= 0x40;
		break;
	default:
		printk(KERN_ERR "via_i2c: specify wrong i2c type.\n");
	}
	seq_iowrite8(VGABASE, adap_data->ioport_index, val);
	spin_unlock_irqrestore(&dev_priv->mmio_lock, flags);
}

int via_get_edid_modes(struct drm_connector *connector)
{
	struct edid *edid = NULL;

	if (connector->edid_blob_ptr)
		edid = (struct edid *) connector->edid_blob_ptr->data;

	return drm_add_edid_modes(connector, edid);
}

static int create_i2c_bus(struct drm_device *dev, struct via_i2c *i2c_par)
{
	struct i2c_adapter *adapter = &i2c_par->adapter;
	struct i2c_algo_bit_data *algo = &i2c_par->algo;

	algo->setsda = via_i2c_setsda;
	algo->setscl = via_i2c_setscl;
	algo->getsda = via_i2c_getsda;
	algo->getscl = via_i2c_getscl;
	algo->udelay = 10;
	algo->timeout = 2;
	algo->data = i2c_par;

	sprintf(adapter->name, "via i2c io_port idx 0x%02x",
		i2c_par->adap_cfg->ioport_index);
	adapter->owner = THIS_MODULE;
	adapter->class = I2C_CLASS_DDC;
	adapter->algo_data = algo;
	adapter->dev.parent = &dev->pdev->dev;
	/* i2c_set_adapdata(adapter, i2c_par); */

	/* Raise SCL and SDA */
	via_i2c_setsda(i2c_par, 1);
	via_i2c_setscl(i2c_par, 1);
	udelay(20);

	return i2c_bit_add_bus(adapter);
}

int via_i2c_init(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	int count = ARRAY_SIZE(adap_configs), ret, i;
	struct via_port_cfg *configs = adap_configs;
	int size = sizeof(struct via_i2c);
	struct via_i2c *i2c = NULL;

	dev_priv->i2c_par = kzalloc(count * size, GFP_KERNEL);
	if (!dev_priv->i2c_par) {
		DRM_ERROR("Failed to allocate VIA i2c data\n");
		return -ENOMEM;
	}
	i2c = dev_priv->i2c_par;

	spin_lock_init(&dev_priv->mmio_lock);
	/*
	 * The OLPC XO-1.5 puts the camera power and reset lines onto
	 * GPIO 2C.
	 */
	if (machine_is_olpc())
		adap_configs[VIA_PORT_2C].mode = VIA_MODE_GPIO;

	for (i = 0; i < count; i++) {
		struct via_port_cfg *adap_cfg = configs++;

		i2c->dev_priv = dev_priv;
		i2c->adap_cfg = NULL;
		if (adap_cfg->type == 0 || adap_cfg->mode != VIA_MODE_I2C) {
			i2c++;
			continue;
		}

		i2c->adap_cfg = adap_cfg;
		ret = create_i2c_bus(dev, i2c);
		if (ret < 0) {
			DRM_ERROR("cannot create i2c bus %u:%d\n", i, ret);
			i2c->adap_cfg = NULL;
			i2c++;
			continue;  /* Still try to make the rest */
		}
		i2c++;
	}
	return 0;
}

void via_i2c_exit(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	struct via_i2c *i2c = dev_priv->i2c_par;
	int i;

	for (i = 0; i < ARRAY_SIZE(adap_configs); i++) {
		/*
		 * Only remove those entries in the array that we've
		 * actually used (and thus initialized algo_data)
		 */
		if (i2c->adap_cfg) {
			i2c_del_adapter(&i2c->adapter);
			i2c->adap_cfg = NULL;
		}
		i2c++;
	}
	kfree(dev_priv->i2c_par);
}
