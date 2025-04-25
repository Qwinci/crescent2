#include "dev/driver.hpp"

static NTSTATUS rtl8168_init(DRIVER_OBJECT* driver, UNICODE_STRING*) {
	return STATUS_SUCCESS;
}

static NTSTATUS rtl8169_add_device(DRIVER_OBJECT* driver, DEVICE_OBJECT* physical_device) {
	return STATUS_SUCCESS;
}

static constexpr kstd::wstring_view COMPATIBLES[] {
	u"PCI\\VEN_10EC&DEV_8139",
	u"PCI\\VEN_10EC&DEV_8168"
};

DRIVER_DECL Driver RTL8168_DRIVER {
	.name = u"rtl8168",
	.compatibles {COMPATIBLES},
	.init = rtl8168_init,
	.add_device = rtl8169_add_device
};
