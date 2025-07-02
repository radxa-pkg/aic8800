-include .github/local/Makefile.local
-include Makefile.extra

PROJECT ?= aic8800
CUSTOM_DEBUILD_ENV ?= DEB_BUILD_OPTIONS='parallel=1'

.DEFAULT_GOAL := all
.PHONY: all
all: build

.PHONY: devcontainer_setup
devcontainer_setup:
	sudo dpkg --add-architecture arm64
	sudo apt-get update
	sudo apt-get build-dep . -y

#
# Test
#
.PHONY: test
test:

#
# Build
#
.PHONY: build
build: pre_build main_build post_build

.PHONY: pre_build
pre_build:
	# Fix file permissions when created from template
	chmod +x debian/rules

.PHONY: main_build
main_build:

.PHONY: post_build
post_build:

#
# Clean
#
.PHONY: distclean
distclean: clean

.PHONY: clean
clean: clean-deb

.PHONY: clean-deb
clean-deb:
	rm -rf debian/.debhelper debian/$(PROJECT)*/ debian/tmp/ debian/debhelper-build-stamp debian/files debian/*.debhelper.log debian/*.*.debhelper debian/*.substvars

#
# Release
#
.PHONY: dch
dch: debian/changelog
	gbp dch --ignore-branch --multimaint-merge --release --spawn-editor=never \
	--git-log='--no-merges --perl-regexp --invert-grep --grep=^(chore:\stemplates\sgenerated)' \
	--dch-opt=--upstream --commit --commit-msg="feat: release %(version)s"

.PHONY: deb
deb: debian
	$(CUSTOM_DEBUILD_ENV) debuild --no-lintian --lintian-hook "lintian --fail-on error,warning --suppress-tags-from-file $(PWD)/debian/common-lintian-overrides -- %p_%v_*.changes" --no-sign -b -aarm64 -Pcross

.PHONY: release
release:
	gh workflow run .github/workflows/new_version.yaml --ref $(shell git branch --show-current)
