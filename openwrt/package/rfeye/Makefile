include $(TOPDIR)/rules.mk

PKG_NAME:=rfeye
PKG_VERSION:=0.1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/rfeye
  SECTION:=net
  CATEGORY:=Network
  TITLE:=AREDN RFeye
  DEPENDS:=+iw +iwinfo
endef

define Package/rfeye/description
  AREDN-native RF spectrum visibility app scaffold for ath10k nodes.
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) -O2 -Wall -Wextra \
		-o $(PKG_BUILD_DIR)/rfeye-spectral-parse \
		./src/rfeye_spectral_parse.c
endef

define Package/rfeye/install
	$(INSTALL_DIR) $(1)/usr/lib/rfeye
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/rfeye-spectral-parse $(1)/usr/lib/rfeye/rfeye-spectral-parse
	$(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,rfeye))
