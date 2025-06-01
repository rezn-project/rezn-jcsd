APP_NAME=reznjcs
APP_NAME_SERVER=reznjcsd
APP_NAME_CLI=reznjcs-cli
VERSION?=$(subst -,~, $(subst v,,$(RAW_VERSION)))
ARCH?=amd64
BUILD_DIR=_build
STAGING_DIR=dist/deb/$(APP_NAME)
DEB_NAME=$(APP_NAME)_$(VERSION)_$(ARCH).deb
CLI_BUILD_DIR=cli

.PHONY: all build-server build-cli stage deb clean

all: build-server build-cli stage deb

build-server:
	mkdir -p $(BUILD_DIR)
	go build -o $(BUILD_DIR)/$(APP_NAME_SERVER) ./cmd/server/main.go

build-cli:
	mkdir -p $(BUILD_DIR)
	go build -o $(BUILD_DIR)/$(APP_NAME_CLI) ./cmd/cli/main.go

stage:
	rm -rf $(STAGING_DIR)
	mkdir -p $(STAGING_DIR)/opt/$(APP_NAME)/bin
	mkdir -p $(STAGING_DIR)/opt/$(APP_NAME_CLI)/bin
	mkdir -p $(STAGING_DIR)/etc/$(APP_NAME)
	mkdir -p $(STAGING_DIR)/lib/systemd/system
	mkdir -p $(STAGING_DIR)/etc/logrotate.d

	cp $(BUILD_DIR)/$(APP_NAME_SERVER) $(STAGING_DIR)/opt/$(APP_NAME_SERVER)/bin/$(APP_NAME_SERVER)
	cp $(BUILD_DIR)/$(APP_NAME_CLI) $(STAGING_DIR)/opt/$(APP_NAME_CLI)/bin/$(APP_NAME_CLI)
	cp packaging/$(APP_NAME).env.default $(STAGING_DIR)/etc/$(APP_NAME)/$(APP_NAME).env.default
	cp packaging/$(APP_NAME).service $(STAGING_DIR)/lib/systemd/system/$(APP_NAME).service
	cp packaging/$(APP_NAME).logrotate $(STAGING_DIR)/etc/logrotate.d/$(APP_NAME)

	cp packaging/postinst.sh postinst.sh
	cp packaging/postrm.sh postrm.sh

	chmod +x postinst.sh postrm.sh

	chmod +x $(STAGING_DIR)/opt/$(APP_NAME)/bin/$(APP_NAME_SERVER)
	chmod +x $(STAGING_DIR)/opt/$(APP_NAME_CLI)/bin/$(APP_NAME_CLI)

deb:
	fpm -s dir -t deb \
	  -n $(APP_NAME) \
	  -v $(VERSION) \
	  -a $(ARCH) \
	  --license "MIT" \
	  --description "Rezn JCS canonicalizes JSON" \
	  --after-install postinst.sh \
	  --after-remove postrm.sh \
	  $(STAGING_DIR)/opt/$(APP_NAME)/=/opt/$(APP_NAME)/ \
	  $(STAGING_DIR)/opt/$(APP_NAME_CLI)/=/opt/$(APP_NAME_CLI)/ \
	  $(STAGING_DIR)/etc/$(APP_NAME)/=/etc/$(APP_NAME)/ \
	  $(STAGING_DIR)/lib/systemd/system/$(APP_NAME).service=/lib/systemd/system/$(APP_NAME).service \
	  $(STAGING_DIR)/etc/logrotate.d/$(APP_NAME)=/etc/logrotate.d/$(APP_NAME)

	rm -f postinst.sh postrm.sh

clean:
	rm -rf dist
	rm -f *.deb
	rm -f postinst.sh postrm.sh