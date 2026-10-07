SHELL := /bin/sh
.DELETE_ON_ERROR:
CC = cc
DYNAMIC_CC ?= $(CC)
GHC ?= ghc
PYTHON ?= python3

CASES := geometry mremap vma-merge shared madvise cow elf-o0 elf-o3 elf-os elf-pie elf-lto \
	dynamic-link dynamic-load
PAYLOADS := launch $(CASES) libwts-test.so

CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 \
	-pthread -fstack-protector-strong
PAGE_FLAGS := -s \
	-Wl,-z,max-page-size=4096,-z,common-page-size=4096,-z,noexecstack
RUNNER_PAGE_FLAGS := \
	-Wl,-z,max-page-size=65536,-z,common-page-size=65536,-z,noexecstack
STATIC_FLAGS := -static -fno-pie -no-pie
DYNAMIC_FLAGS := -fPIE -pie -Wl,-z,lazy,-rpath,'$$ORIGIN'

.PHONY: all check package clean
all: dist/wts

build dist:
	mkdir -p $@

build/launch: runner/launch.c Makefile | build
	$(CC) $(CFLAGS) $(STATIC_FLAGS) -s $(RUNNER_PAGE_FLAGS) $< -o $@

define case
build/$(1): c/$(2).c c/common.c c/common.h Makefile | build
	$(CC) $(CFLAGS) $(3) $(PAGE_FLAGS) c/common.c c/$(2).c -o $$@
endef
$(eval $(call case,geometry,geometry,$(STATIC_FLAGS)))
$(eval $(call case,mremap,mremap,$(STATIC_FLAGS)))
$(eval $(call case,vma-merge,vma-merge,$(STATIC_FLAGS)))
$(eval $(call case,shared,shared,$(STATIC_FLAGS)))
$(eval $(call case,madvise,madvise,$(STATIC_FLAGS)))
$(eval $(call case,cow,cow,$(STATIC_FLAGS)))
$(eval $(call case,elf-o0,elf,$(STATIC_FLAGS) -O0))
$(eval $(call case,elf-o3,elf,$(STATIC_FLAGS) -O3))
$(eval $(call case,elf-os,elf,$(STATIC_FLAGS) -Os))
$(eval $(call case,elf-pie,elf,-static-pie -fPIE))
$(eval $(call case,elf-lto,elf,$(STATIC_FLAGS) -O3 -flto))

build/libwts-test.so: c/library.c c/library.h Makefile | build
	$(DYNAMIC_CC) $(CFLAGS) $(PAGE_FLAGS) -fPIC -shared \
		-Wl,-z,lazy,-soname,libwts-test.so c/library.c -o $@

build/dynamic-link: c/dynamic.c c/library.h c/common.c c/common.h \
		build/libwts-test.so Makefile | build
	$(DYNAMIC_CC) $(CFLAGS) $(PAGE_FLAGS) $(DYNAMIC_FLAGS) \
		c/common.c c/dynamic.c -Lbuild -lwts-test -o $@

build/dynamic-load: c/dynamic.c c/library.h c/common.c c/common.h Makefile | build
	$(DYNAMIC_CC) $(CFLAGS) $(PAGE_FLAGS) $(DYNAMIC_FLAGS) \
		-DWTS_DLOPEN c/common.c c/dynamic.c -ldl -o $@

build/embed.stamp: $(addprefix build/,$(PAYLOADS)) embed.py
	$(PYTHON) embed.py $(PAYLOADS)
	touch $@

dist/wts: build/embed.stamp runner/Main.hs runner/bridge.c Makefile | dist
	$(CC) -c build/payload.S -o build/payload-asm.o
	$(CC) -c build/payload.c -o build/payload-table.o
	$(CC) $(CFLAGS) -c runner/bridge.c -o build/bridge.o
	$(GHC) -O2 -Wall -Werror -threaded -ibuild -outputdir build \
		runner/Main.hs build/payload-asm.o build/payload-table.o build/bridge.o \
		-static -optl-static -optl-pthread -optl-no-pie -optl-s \
		$(addprefix -optl,$(RUNNER_PAGE_FLAGS)) \
		-o $@

check: all
	./dist/wts

package: all
	rm -rf dist/wts-linux-aarch64
	mkdir -p dist/wts-linux-aarch64/source \
		dist/wts-linux-aarch64/relink \
		dist/wts-linux-aarch64/notices
	cp dist/wts README.md dist/wts-linux-aarch64/
	cp -R Makefile embed.py c runner .github README.md dist/wts-linux-aarch64/source/
	cp build/*.o build/Payload.hs dist/wts-linux-aarch64/relink/
	for p in ghc libc6 libgmp10 libffi8 libnuma1; do \
		if test -f /usr/share/doc/$$p/copyright; then \
			cp /usr/share/doc/$$p/copyright \
				dist/wts-linux-aarch64/notices/$$p || exit 1; \
		fi; \
	done
	tar -czf dist/wts-linux-aarch64.tar.gz -C dist wts-linux-aarch64
	cd dist && sha256sum wts-linux-aarch64.tar.gz > SHA256SUMS

clean:
	rm -rf build dist
