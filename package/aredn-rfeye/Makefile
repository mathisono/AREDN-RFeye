include $(TOPDIR)/rules.mk

PKG_NAME:=aredn-rfeye
PKG_VERSION:=0.1.0
PKG_RELEASE:=2
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
	$(TARGET_CC) $(TARGET_CFLAGS) -O2 -Wall -Wextra \
		-o $(PKG_BUILD_DIR)/rfeye-spectral-parse \
		./src/rfeye_spectral_parse.c
endef

define Package/aredn-rfeye/install
	$(INSTALL_DIR) $(1)/usr/lib/rfeye
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/rfeye-spectral-parse $(1)/usr/lib/rfeye/rfeye-spectral-parse
	[ ! -f ./files/usr/lib/rfeye/rfeye-probe.sh ] || $(INSTALL_BIN) ./files/usr/lib/rfeye/rfeye-probe.sh $(1)/usr/lib/rfeye/rfeye-probe.sh

	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) ./files/usr/sbin/rfeye-agent $(1)/usr/sbin/rfeye-agent
	$(INSTALL_BIN) ./files/usr/sbin/rfeye-survey $(1)/usr/sbin/rfeye-survey

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/rfeye $(1)/etc/init.d/rfeye

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/rfeye $(1)/etc/config/rfeye

	$(INSTALL_DIR) $(1)/www/cgi-bin/apps/rfeye
	$(INSTALL_BIN) ./files/www/cgi-bin/apps/rfeye/user $(1)/www/cgi-bin/apps/rfeye/user

	$(INSTALL_DIR) $(1)/www/cgi-bin/apps/rfeye/data
	$(INSTALL_BIN) ./files/www/cgi-bin/apps/rfeye/data/agent.sh $(1)/www/cgi-bin/apps/rfeye/data/agent.sh

	[ ! -d ./files/www/apps ] || $(CP) ./files/www/apps $(1)/www/
endef

$(eval $(call BuildPackage,aredn-rfeye))
