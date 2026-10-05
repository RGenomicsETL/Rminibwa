# h/t to @jimhester and @yihui for this parse block:
# https://github.com/yihui/knitr/blob/dc5ead7bcfc0ebd2789fe99c527c7d91afb3de4a/Makefile#L1-L4
# Note the portability change as suggested in the manual:
# https://cran.r-project.org/doc/manuals/r-release/R-exts.html#Writing-portable-packages
PKGNAME := $(shell sed -n 's/Package: *\([^ ]*\)/\1/p' DESCRIPTION)
PKGVERS := $(shell sed -n 's/Version: *\([^ ]*\)/\1/p' DESCRIPTION)
MINIBWA_BINDINGS_ROOT ?=
RUSTFLAGS_AVX2 ?= -C target-feature=+avx2
MINIBWA_NATIVE_CFLAGS ?= -DHAVE_KALLOC
RMINIBWA_BENCH_PYTHONPATH ?= $(CURDIR)/bench/python
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
	@rm -rf $(PKGNAME)_$(PKGVERS).tar.gz $(PKGNAME)_$(PKGVERS).tgz $(PKGNAME).Rcheck .Rcheck src/vendor/_archives config.log src/*.o src/*.so src/*.dll src/*.dylib bench/python

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

check-bench-python-root:
	@test -n "$(MINIBWA_BINDINGS_ROOT)" || \
		(echo 'Set MINIBWA_BINDINGS_ROOT to a local minibwa bindings checkout for external benchmarks.' && exit 1)
	@test -d "$(MINIBWA_BINDINGS_ROOT)/minibwa-py" || \
		(echo 'Missing Python binding at $(MINIBWA_BINDINGS_ROOT)/minibwa-py.' && exit 1)

bench-python: check-bench-python-root
	rm -rf $(RMINIBWA_BENCH_PYTHONPATH)
	mkdir -p $(RMINIBWA_BENCH_PYTHONPATH)
	cd $(MINIBWA_BINDINGS_ROOT)/minibwa-py && \
		CFLAGS='$(MINIBWA_NATIVE_CFLAGS)' RUSTFLAGS='$(RUSTFLAGS_AVX2)' python3 -m pip install --root-user-action=ignore --no-deps --force-reinstall --target $(RMINIBWA_BENCH_PYTHONPATH) .

test-bench-env:
	test -d $(RMINIBWA_BENCH_PYTHONPATH)

rdm: dev-install bench-python test-bench-env
	RMINIBWA_RUN_BENCHMARKS=true \
	RETICULATE_PYTHON=$(RETICULATE_PYTHON) \
	RMINIBWA_BENCH_PYTHONPATH=$(RMINIBWA_BENCH_PYTHONPATH) \
	R -e 'rmarkdown::render("README.Rmd", output_format = rmarkdown::github_document(), output_file = "README.md")'

.PHONY: all rd readme vig vig-md pkgdown vendor vendor-status sync-upstream check-upstream-sync minibwa-cli build check install_deps install install2 install3 clean dev-install test1 test2 test0 test asm check-bench-python-root bench-python test-bench-env rdm
