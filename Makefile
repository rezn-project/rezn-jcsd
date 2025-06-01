APP_NAME=reznjcs
APP_NAME_DAEMON=reznjcsd
APP_NAME_CLI=reznjcs-cli
VERSION?=$(subst -,~, $(subst v,,$(RAW_VERSION)))
ARCH?=amd64
BUILD_DIR=build
STAGING_DIR=dist/deb/$(APP_NAME)
DEB_NAME=$(APP_NAME)_$(VERSION)_$(ARCH).deb
CLI_BUILD_DIR=cli

.PHONY: all build-server build-cli stage deb clean

all: build-server build-cli stage deb

build-server:
	mkdir -p $(BUILD_DIR)
	go build -o $(BUILD_DIR)/$(APP_NAME_DAEMON) ./cmd/jcsd/main.go

build-cli:
	mkdir -p $(BUILD_DIR)
	go build -o $(BUILD_DIR)/$(APP_NAME_CLI) ./cmd/cli/main.go

stage:
	rm -rf $(STAGING_DIR)
	mkdir -p $(STAGING_DIR)/opt/$(APP_NAME_CLI)/bin
	mkdir -p $(STAGING_DIR)/opt/$(APP_NAME_DAEMON)/bin
	mkdir -p $(STAGING_DIR)/etc/$(APP_NAME_DAEMON)
	mkdir -p $(STAGING_DIR)/lib/systemd/system
	mkdir -p $(STAGING_DIR)/etc/logrotate.d

	cp $(BUILD_DIR)/$(APP_NAME_DAEMON) $(STAGING_DIR)/opt/$(APP_NAME_DAEMON)/bin/$(APP_NAME_DAEMON)
	cp $(BUILD_DIR)/$(APP_NAME_CLI) $(STAGING_DIR)/opt/$(APP_NAME_CLI)/bin/$(APP_NAME_CLI)
	cp packaging/$(APP_NAME_DAEMON).env.default $(STAGING_DIR)/etc/$(APP_NAME_DAEMON)/$(APP_NAME_DAEMON).env.default
	cp packaging/$(APP_NAME_DAEMON).service $(STAGING_DIR)/lib/systemd/system/$(APP_NAME_DAEMON).service
	cp packaging/$(APP_NAME_DAEMON).logrotate $(STAGING_DIR)/etc/logrotate.d/$(APP_NAME_DAEMON)

	cp packaging/postinst.sh postinst.sh
	cp packaging/postrm.sh postrm.sh

	chmod +x postinst.sh postrm.sh

	chmod +x $(STAGING_DIR)/opt/$(APP_NAME_DAEMON)/bin/$(APP_NAME_DAEMON)
	chmod +x $(STAGING_DIR)/opt/$(APP_NAME_CLI)/bin/$(APP_NAME_CLI)

deb:
	fpm -s dir -t deb \
	  -n $(APP_NAME_DAEMON) \
	  -v $(VERSION) \
	  -a $(ARCH) \
	  --license "MIT" \
	  --description "Rezn JCS canonicalizes JSON" \
	  --after-install postinst.sh \
	  --after-remove postrm.sh \
	  $(STAGING_DIR)/opt/$(APP_NAME_DAEMON)/=/opt/$(APP_NAME)/ \
	  $(STAGING_DIR)/opt/$(APP_NAME_CLI)/=/opt/$(APP_NAME_CLI)/ \
	  $(STAGING_DIR)/etc/$(APP_NAME_DAEMON)/=/etc/$(APP_NAME_DAEMON)/ \
	  $(STAGING_DIR)/lib/systemd/system/$(APP_NAME_DAEMON).service=/lib/systemd/system/$(APP_NAME_DAEMON).service \
	  $(STAGING_DIR)/etc/logrotate.d/$(APP_NAME_DAEMON)=/etc/logrotate.d/$(APP_NAME_DAEMON)

	rm -f postinst.sh postrm.sh

clean:
	rm -rf dist
	rm -f *.deb
	rm -f postinst.sh postrm.sh