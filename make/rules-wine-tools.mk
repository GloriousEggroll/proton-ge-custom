# parameters:
#   $(1): lowercase package name
#   $(2): uppercase package name
#   $(3): build target arch
#   $(4): build target os
#
define create-rules-wine-tools
ifneq ($(findstring $(3)-unix,$(ARCHS)),)

$$(OBJ)/.$(1)-$(3)-tools: CCACHE_BASEDIR = $$($(2)_SRC)
$$(OBJ)/.$(1)-$(3)-tools: $$(OBJ)/.$(1)-source
$$(OBJ)/.$(1)-$(3)-tools: $$(OBJ)/.$(1)-$(3)-configure

ifneq ($(3),$$(HOST_ARCH))
$$(OBJ)/.$(1)-$(3)-configure: | $$(OBJ)/.$(1)-$$(HOST_ARCH)-tools
$$(OBJ)/.$(1)-$(3)-build: $$(OBJ)/.$(1)-$$(HOST_ARCH)-tools
$(2)_$(3)_AUTOCONF_ARGS += --with-wine-tools=$$($(2)_$$(HOST_ARCH)_OBJ)
endif

$$(OBJ)/.$(1)-$(3)-tools:
ifeq ($(3),$$(HOST_ARCH))
	@echo ":: building $(1)-$(3)-tools..." >&2
	+cd "$$($(2)_$(3)_OBJ)" && env $$($(2)_$(3)_ENV) \
	$$(MAKE) __tooldeps__
endif
	touch $$@

$$(OBJ)/.$(1)-$(3)-build: $$(OBJ)/.$(1)-$(3)-tools

endif
endef

rules-wine-tools = $(call create-rules-wine-tools,$(1),$(call toupper,$(1)),$(2),$(3))
make-builtin = $(WINE_$(HOST_ARCH)_OBJ)/tools/winebuild/winebuild --builtin $(1)
