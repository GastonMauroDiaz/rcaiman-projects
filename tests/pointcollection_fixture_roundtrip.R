# This test uses R base only. It does not load rcaiman or any other package.
main <- function() {
  args <- commandArgs(trailingOnly = TRUE)
  stopifnot(length(args) == 2L)
  executable <- normalizePath(args[1], winslash = "/", mustWork = TRUE)
  fixture <- normalizePath(args[2], winslash = "/", mustWork = TRUE)
  output <- tempfile("rcaiman-projects-points-v3-")
  dir.create(output)
  on.exit(unlink(output, recursive = TRUE), add = TRUE)

  status <- system2(executable, shQuote(output))
  if (status != 0L)
    stop("The C++ fixture test failed: ", status)

  original <- read.csv(fixture, check.names = FALSE)
  unchanged <- read.csv(file.path(output, "unchanged.csv"), check.names = FALSE)
  edited <- read.csv(file.path(output, "edited.csv"), check.names = FALSE)
  ratio <- original[["ratio with sky_cie"]]
  stopifnot(
    nrow(original) == 100L,
    identical(names(original), c("x", "y_down", "col", "row", "Red", "Green", "Blue",
                                 "mask 10", "ratio with sky_cie")),
    is.logical(original[["mask 10"]]),
    is.double(ratio),
    sum(is.na(ratio)) == 87L,
    sum(is.infinite(ratio) & ratio > 0, na.rm = TRUE) == 2L,
    sum(is.finite(ratio)) == 11L,
    identical(original, unchanged)
  )
  expected <- original
  expected$x[1] <- expected$x[1] + 1.25
  expected$y_down[1] <- expected$y_down[1] - 2.25
  stopifnot(identical(expected, edited))
  stopifnot(identical(read.csv(fixture), read.csv(file.path(output, "unchanged.csv"))))
  cat("R base: all values, 87 NA, 2 Inf, logical types and column names preserved.\n")
}

main()
