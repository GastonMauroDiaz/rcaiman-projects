# Point CSV interoperability tests

`data/points_v3_numeric.csv` is an unchanged copy of the user-provided `test.csv`,
generated with core v3 `write_points()`. It contains 100 points and these columns:
`x,y_down,col,row,Red,Green,Blue,mask 10,ratio with sky_cie`.
The mask column is logical; the ratio column contains 87 `NA`, two positive `Inf`
and 11 finite numbers. Two column names contain spaces.

`pointcollection_fixture_tests` verifies the schema, all 900 values,
cell-center/index correspondence, and two save/reload paths: unchanged points and
one moved point. Additional columns (including `col,row`) remain script-provided
attributes; editing coordinates does not recompute them. The fixture is never
overwritten. Existing `pointcollection_tests` still cover legacy-header rejection.

When CMake can find `Rscript` (or `RSCRIPT_EXECUTABLE` is set), CTest also runs
`pointcollection_r_roundtrip_tests`. This uses R base `read.csv()` and `identical()`
to check both generated files, including column types, missing and infinite values.
It checks names both verbatim (`check.names = FALSE`) and with R's default name
normalization. It does not load rcaiman.
R is optional and is not a dependency of the application or the C++ tests.

This fixture covers the intended numeric/logical workflow. It does not test textual
attributes, embedded quotes/newlines, negative infinity, NaN or full double precision.
