SRC ?= .
DST ?= .

.PHONY: all
all: $(DST)/d3d8.dll $(DST)/d3d9.dll $(DST)/d3d10core.dll $(DST)/d3d11.dll $(DST)/dxgi.dll $(DST)/d3d12.dll $(DST)/d3d12core.dll

$(DST)/load_config_aarch64.o: $(SRC)/load_config.S
	clang --target=aarch64-windows -c -o $@ $^

$(DST)/load_config_arm64ec.o: $(SRC)/load_config.S
	clang --target=arm64ec-windows -c -o $@ $^

$(DST)/%.dll: $(SRC)/%_aarch64.def $(SRC)/%_arm64ec.def $(DST)/load_config_aarch64.o $(DST)/load_config_arm64ec.o
	lld-link /dll /noimplib /noentry /machine:arm64x \
	    /defArm64Native:$(word 1,$^) \
	    /def:$(word 2,$^) \
	    /out:$@ \
	    load_config_aarch64.o \
	    load_config_arm64ec.o

