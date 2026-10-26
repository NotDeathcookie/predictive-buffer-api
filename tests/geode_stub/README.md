# geode_stub - NOT the Geode SDK

A tiny fake of the handful of Geode/GD declarations that `src/geode/*` and
`include/pbuf/Api.hpp` use, so a plain g++ can type-check that code for typos and
internal inconsistencies (`tools/syntax_check_stub.sh`).

It is modeled on how the ORIGINAL project and the Geode docs use these APIs. It
proves nothing about whether the real SDK accepts the code. It exists because "I
read it carefully" is a weaker guarantee than "it parses and type-checks against a
written-down set of assumptions". `Hooks.cpp` ($modify) is not covered.
