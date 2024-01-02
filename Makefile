PROJECT ?= aic8800
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
MANDIR ?= $(PREFIX)/share/man

.PHONY: all
all: build

#
# Test
#
.PHONY: test
test:

#
# Build
#
.PHONY: build
build: build-doc build-aicrf-test

SRC-DOC		:=	.
DOCS		:=	$(SRC-DOC)/SOURCE
.PHONY: build-doc
build-doc: $(DOCS)

$(SRC-DOC):
	mkdir -p $(SRC-DOC)

.PHONY: $(SRC-DOC)/SOURCE
$(SRC-DOC)/SOURCE: $(SRC-DOC)
	echo -e "git clone $(shell git remote get-url origin)\ngit checkout $(shell git rev-parse HEAD)" > "$@"

SRC-AICRF-TEST	:=	src/tools/aicrf_test/
BIN-AICRF-TEST	:=	bt_test wifi_test
BINS-AICRF-TEST	:=	$(addprefix $(SRC-AICRF-TEST),$(BIN-AICRF-TEST))
.PHONY: build-aicrf-test
build-aicrf-test: $(BINS-AICRF-TEST)
	make CROSS_COMPILE=aarch64-linux-gnu- -C $(SRC-AICRF-TEST)

#
# Clean
#
.PHONY: distclean
distclean: clean

.PHONY: clean
clean: clean-doc clean-deb clean-aicrf-test

.PHONY: clean-doc
clean-doc:
	rm -rf $(DOCS)

.PHONY: clean-deb
clean-deb:
	rm -rf debian/.debhelper debian/aic8800-dkms debian/aicrf-test debian/debhelper-build-stamp debian/files debian/*.debhelper.log debian/*.*.debhelper debian/*.substvars

.PHONY: clean-aicrf-test
clean-aicrf-test:
	make -C $(SRC-AICRF-TEST) -j$(shell nproc) clean || true

#
# Release
#
.PHONY: dch
dch: debian/changelog
	gbp dch --debian-branch=main

.PHONY: deb
deb: debian
	debuild --no-lintian --lintian-hook "lintian --fail-on error,warning --suppress-tags bad-distribution-in-changes-file -- %p_%v_*.changes" --no-sign -b -aarm64 -Pcross
