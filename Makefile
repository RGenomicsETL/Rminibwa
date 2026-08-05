# h/t to @jimhester and @yihui for this parse block:
# https://github.com/yihui/knitr/blob/dc5ead7bcfc0ebd2789fe99c527c7d91afb3de4a/Makefile#L1-L4
# Note the portability change as suggested in the manual:
# https://cran.r-project.org/doc/manuals/r-release/R-exts.html#Writing-portable-packages
PKGNAME := $(shell sed -n 's/Package: *\([^ ]*\)/\1/p' DESCRIPTION)
PKGVERS := $(shell sed -n 's/Version: *\([^ ]*\)/\1/p' DESCRIPTION)
MINIBWA_BINDINGS_ROOT ?=
MINIBWA_RUST_CRATE ?= $(MINIBWA_BINDINGS_ROOT)/minibwa
RUSTFLAGS_AVX2 ?= -C target-feature=+avx2
MINIBWA_NATIVE_CFLAGS ?= -DHAVE_KALLOC
RMINIBWA_BENCH_PYTHONPATH ?= $(CURDIR)/bench/python
RMINIBWA_BENCH_RUST_LIB ?= $(CURDIR)/bench/rust-shim/target/release/librminibwa_bench_rust.so
RETICULATE_PYTHON ?= $(shell command -v python3)

all: check

rd:
	R -e 'roxygen2::roxygenize(load_code = "source")'

readme:
	R -e 'rmarkdown::render("README.Rmd", output_format = rmarkdown::github_document(), output_file = "README.md")'

vig:
	R -e "tools::buildVignettes(dir = '.')"

vig-md:
	R -e "for (f in Sys.glob('vignettes/*.Rmd')) { out <- sub('\\\\.Rmd$$', '.md', f); rmarkdown::render(f, output_format = rmarkdown::md_document(variant = 'gfm'), output_file = basename(out), output_dir = dirname(out), quiet = FALSE, envir = new.env(parent = globalenv())) }"

pkgdown:
	R -e 'pkgdown::build_site()'

vendor:
	Rscript tools/vendor-minibwa.R refresh

vendor-status:
	Rscript tools/vendor-minibwa.R status

sync-upstream:
	@mkdir -p inst/upstream
	@cp tools/minibwa-upstream.dcf inst/upstream/minibwa.dcf
	@echo 'Upstream metadata synced.'

check-upstream-sync:
	@diff -q tools/minibwa-upstream.dcf inst/upstream/minibwa.dcf >/dev/null || \
		(echo 'ERROR: upstream metadata drift detected' && exit 1)

minibwa-cli:
	tools/build-minibwa-cli.sh

build:
	R CMD build .

check: build
	R CMD check --as-cran --no-manual $(PKGNAME)_$(PKGVERS).tar.gz

install_deps:
	R \
	-e 'options(repos = c(RGenomicsETL = "https://rgenomicsetl.r-universe.dev", sounkou = "https://sounkou-bioinfo.r-universe.dev", CRAN = "https://cloud.r-project.org"))' \
	-e 'if (!requireNamespace("remotes", quietly = TRUE)) install.packages("remotes")' \
	-e 'remotes::install_deps(dependencies = TRUE)'

install: build
	R CMD INSTALL $(PKGNAME)_$(PKGVERS).tar.gz

install2:
	R CMD INSTALL --no-configure .

install3:
	R CMD INSTALL .

clean:
	@rm -rf $(PKGNAME)_$(PKGVERS).tar.gz $(PKGNAME)_$(PKGVERS).tgz $(PKGNAME).Rcheck .Rcheck src/vendor/_archives config.log src/*.o src/*.so src/*.dll src/*.dylib bench/python bench/rust-shim/target bench/rust-shim/Cargo.lock bench/rust-shim/Cargo.toml

# Development targets
dev-install:
	R CMD INSTALL --preclean .

test1:
	R -e "tinytest::test_package('$(PKGNAME)', testdir = 'inst/tinytest', ncpu = 1L)"

test2:
	R -e "tinytest::test_package('$(PKGNAME)', testdir = 'inst/tinytest', ncpu = 2L)"

test0:
	R -e "tinytest::test_package('$(PKGNAME)', testdir = 'inst/tinytest')"

test: install
	R -e "tinytest::test_package('$(PKGNAME)', testdir = 'inst/tinytest')"

asm: dev-install
	Rscript tools/check-assembly.R .

check-bench-bindings-root:
	@test -n "$(MINIBWA_BINDINGS_ROOT)" || \
		(echo 'Set MINIBWA_BINDINGS_ROOT to a local minibwa bindings checkout for external benchmarks.' && exit 1)
	@test -d "$(MINIBWA_BINDINGS_ROOT)/minibwa-py" || \
		(echo 'Missing Python binding at $(MINIBWA_BINDINGS_ROOT)/minibwa-py.' && exit 1)
	@test -d "$(MINIBWA_RUST_CRATE)" || \
		(echo 'Missing Rust minibwa crate at $(MINIBWA_RUST_CRATE).' && exit 1)

bench-python: check-bench-bindings-root
	rm -rf $(RMINIBWA_BENCH_PYTHONPATH)
	mkdir -p $(RMINIBWA_BENCH_PYTHONPATH)
	cd $(MINIBWA_BINDINGS_ROOT)/minibwa-py && \
		CFLAGS='$(MINIBWA_NATIVE_CFLAGS)' RUSTFLAGS='$(RUSTFLAGS_AVX2)' python3 -m pip install --root-user-action=ignore --no-deps --force-reinstall --target $(RMINIBWA_BENCH_PYTHONPATH) .

bench-rust: check-bench-bindings-root
	MINIBWA_RUST_CRATE='$(MINIBWA_RUST_CRATE)' Rscript -e 'x <- readLines("bench/rust-shim/Cargo.toml.in"); x <- sub("@MINIBWA_RUST_CRATE@", normalizePath(Sys.getenv("MINIBWA_RUST_CRATE"), winslash = "/", mustWork = TRUE), x, fixed = TRUE); writeLines(x, "bench/rust-shim/Cargo.toml")'
	cd bench/rust-shim && CFLAGS='$(MINIBWA_NATIVE_CFLAGS)' RUSTFLAGS='$(RUSTFLAGS_AVX2)' cargo build --release

test-bench-env:
	test -d $(RMINIBWA_BENCH_PYTHONPATH)
	test -f $(RMINIBWA_BENCH_RUST_LIB)

rdm: dev-install bench-python bench-rust test-bench-env
	RMINIBWA_RUN_BENCHMARKS=true \
	RETICULATE_PYTHON=$(RETICULATE_PYTHON) \
	RMINIBWA_BENCH_PYTHONPATH=$(RMINIBWA_BENCH_PYTHONPATH) \
	RMINIBWA_BENCH_RUST_LIB=$(RMINIBWA_BENCH_RUST_LIB) \
	R -e 'rmarkdown::render("README.Rmd", output_format = rmarkdown::github_document(), output_file = "README.md")'

.PHONY: all rd readme vig vig-md pkgdown vendor vendor-status sync-upstream check-upstream-sync minibwa-cli build check install_deps install install2 install3 clean dev-install test1 test2 test0 test asm check-bench-bindings-root bench-python bench-rust test-bench-env rdm
