# XML parser cleanup regression tests

Standalone tests using the real parser and tokenizer. No game assets or new dependencies are required.

```sh
cmake -S tests/xml_parser -B build/xml-parser-tests
cmake --build build/xml-parser-tests
ctest --test-dir build/xml-parser-tests --output-on-failure
```

For multi-configuration generators, pass the chosen configuration to the build and CTest commands.

The test includes `src/core/xml_parser.c` with allocation tracking wrappers, while retaining its production parsing logic. It covers valid documents, incompatible closing tags, truncated input, rejected child elements, repeated failures, reset after success/failure, all four allocation failures during initialization, initialization with an invalid element definition, and recovery. Newly allocated memory is filled with a nonzero pattern to expose cleanup of uninitialized pointers. Failed tests release tracked leftovers only after measuring them, keeping cases independent.

To compare against an older parser, configure with `-DXML_PARSER_SOURCE=/absolute/path/to/xml_parser.c`; its matching `xml_parser.h` must be beside that source. The tests expect no outstanding parser allocations and no text carried from an earlier document. They do not validate all XML syntax, allocation failures during text accumulation, or the full game lifecycle.
