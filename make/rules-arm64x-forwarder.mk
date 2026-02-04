# parameters:
#   $(1): lowercase package name
#   $(2): uppercase package name
define create-rules-arm64x-forwarders
$(2)_OBJ := $$(OBJ)/obj-$(1)

$$($(2)_OBJ):
	mkdir -p $$@

$$(OBJ)/.$(1)-build: $$(OBJ)/.$(1)-source
$$(OBJ)/.$(1)-build: | $$($(2)_OBJ)
	cd "$$($(2)_OBJ)" && $$(MAKE) --file $$($(2)_SRC)/Makefile SRC=$$($(2)_SRC) DST=$$($(2)_OBJ) all
	touch $$@

$$(OBJ)/.$(1)-post-build: $$(OBJ)/.$(1)-build

$(1)-build: $$(OBJ)/.$(1)-build
all-build: $(1)-build
.PHONY: all-build

all-dist: $$(OBJ)/.$(1)-post-build
.PHONY: all-dist
endef

rules-arm64x-forwarders = $(call create-rules-arm64x-forwarders,$(1),$(call toupper,$(1)))
