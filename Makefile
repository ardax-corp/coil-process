# coil-process — build the FFI artifact and run package tests.
#
# Targets:
#   make / make native  — build native/libprocess.{so,dylib,dll}
#   make smoke          — C ABI smoke test (after native)
#   make test           — coil language harness (needs `coil` on PATH)
#   make clean          — remove build products

.PHONY: all native smoke test clean dist

COIL ?= coil

all: native

native:
	$(MAKE) -C native artifact

smoke: native
	$(MAKE) -C native smoke

# Host grant is CLI-only. Runtime dload still needs a native sha256 pin.
test: native
	@lib=native/libprocess.so; \
	  if [ -f native/libprocess.dylib ]; then lib=native/libprocess.dylib; fi; \
	  if [ -f native/libprocess.dll ]; then lib=native/libprocess.dll; fi; \
	  if command -v sha256sum >/dev/null 2>&1; then hash=$$(sha256sum "$$lib" | awk '{print $$1}'); \
	  else hash=$$(shasum -a 256 "$$lib" | awk '{print $$1}'); fi; \
	  printf '%s\n' '[[package]]' "name = 'process'" '[[package.native]]' "sha256 = '$$hash'" > coil.lock
	$(COIL) test --allow-dload process --ffi-search-path native

clean:
	$(MAKE) -C native clean

dist: native
	@$(MAKE) -C native print-artifact
