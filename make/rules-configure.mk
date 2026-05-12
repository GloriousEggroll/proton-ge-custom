# parameters:
#   $(1): lowercase package name
#   $(2): uppercase package name
#   $(3): build target arch
#   $(4): build target os
#
define create-rules-configure
$(call create-rules-common,$(1),$(2),$(3),$(4))
ifneq ($(findstring $(3)-$(4),$(ARCHS)),)

ifeq ($(wildcard $($(2)_ORIGIN)/configure),)
$(2)_CONFIGURE_DEPS = $$($(2)_SRC)/configure

$$($(2)_SRC)/configure: $$($(2)_ORIGIN)/configure.ac | $$(OBJ)/.$(1)-post-source
	@echo ":: autoreconfing $(1)..." >&2
	cd "$$($(2)_SRC)" && autoreconf -fiv
endif

$$(OBJ)/.$(1)-$(3)-configure: $$($(2)_CONFIGURE_DEPS)
	@echo ":: configuring $(1)-$(3)..." >&2

	cd "$$($(2)_$(3)_OBJ)" && env $$($(2)_$(3)_ENV) \
	$$($(2)_SRC)/configure $(--quiet?) \
	    --prefix="$$($(2)_$(3)_DST)" \
	    --libdir="$$($(2)_$(3)_LIBDIR)/$$($(3)-$(4)_LIBDIR)" \
	    $$($(2)_$(3)-$(4)_CONFIGURE_ARGS) \
	    $$($(2)_CONFIGURE_ARGS) \
	    $$($(2)_$(3)_CONFIGURE_ARGS)

	touch $$@

$$(OBJ)/.$(1)-$(3)-build:
	@echo ":: building $(1)-$(3)..." >&2
	+cd "$$($(2)_$(3)_OBJ)" && env $$($(2)_$(3)_ENV) \
	$$(MAKE)
	cd "$$($(2)_$(3)_OBJ)" && env $$($(2)_$(3)_ENV) \
	$$(MAKE) install
	touch $$@
endif
endef

rules-configure = $(call create-rules-configure,$(1),$(call toupper,$(1)),$(2),$(3))
