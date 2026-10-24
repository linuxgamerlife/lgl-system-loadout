NAME := lgl-system-loadout
SPEC := packaging/$(NAME).spec
VERSION := $(shell awk '/^Version:/ { print $$2; exit }' $(SPEC))
SOURCE_ARCHIVE := $(NAME)-$(VERSION).tar.gz
SOURCE_FILES := .copr .gitignore CHANGELOG.md CMakeLists.txt HELP.md LICENSE Makefile README.md \
	data docs packaging src tests

.DEFAULT_GOAL := srpm

outdir ?= $(CURDIR)/result
RPMBUILD ?= rpmbuild

.PHONY: source srpm clean

source:
	@test -n "$(VERSION)" || { echo "Could not read Version from $(SPEC)"; exit 1; }
	tar --create --gzip \
		--file=$(SOURCE_ARCHIVE).tmp \
		--sort=name \
		--owner=0 --group=0 --numeric-owner \
		--exclude='build' --exclude='build-*' --exclude='dist' --exclude='result' --exclude='legacy' \
		--transform='s,^,$(NAME)-$(VERSION)/,' \
		$(SOURCE_FILES)
	mv $(SOURCE_ARCHIVE).tmp $(SOURCE_ARCHIVE)

srpm: source
	mkdir -p $(outdir)
	$(RPMBUILD) -bs $(SPEC) \
		--define "_sourcedir $(CURDIR)" \
		--define "_srcrpmdir $(abspath $(outdir))"

clean:
	rm -f $(SOURCE_ARCHIVE) $(SOURCE_ARCHIVE).tmp
