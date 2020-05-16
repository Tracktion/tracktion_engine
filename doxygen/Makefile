SHELL := /bin/bash

SOURCE_FILES := $(shell find ../modules -type f -name "tracktion_*.h" -or -name "tracktion_*.dox" | sed 's/ /\\ /g')

.PHONEY: clean

doc/index.html: build/tracktion_modules.dox Doxyfile
	doxygen

build/tracktion_modules.dox: process_source_files.py $(SOURCE_FILES)
	python $< ../modules build

clean:
	rm -rf build doc

