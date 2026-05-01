include $(TOPDIR)/rules.mk

PKG_NAME:=aredn-rfeye
PKG_VERSION:=0.1.0
PKG_RELEASE:=1
PKG_LICENSE:=GPL-3.0-or-later
PKG_MAINTAINER:=Mathison Ott

include $(INCLUDE_DIR)/package.mk

define Package/aredn-rfeye
  SECTION:=aredn
  CATEGORY:=AREDN
  TITLE:=AREDN RFeye spectral monitor scaffold
  DEPENDS:=+iw +uhttpd
endef

define Package/aredn-rfeye/description
  AREDN-native RF spectrum visibility app scaffold. Uses ath10k spectral
  debugfs hooks when OpenWrt is built with ATH_DEBUG and ATH_SPECTRAL.
endef

define Build/Compile
endef

define Package/aredn-rfeye/install
	$(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,aredn-rfeye))
