# Radiometric crosscalibration.

# Note: this is not and automatic process, use the script as a template.

require(rcaiman.registry)
rm(list = ls())

# parameters --------------------------------------------------------------

my_entry <- readRDS("_registry_entry/Nikon_D610.Nikkor_8mm.CIEFAP_v2.rds")
spec <- get_band_spec(my_entry, "raw", "bayer")
geometry <- get_geometry_spec(spec, "simple_method")


# Image preprocessing -----------------------------------------------------
source("scripts/00_helpers.R")

files <- dir("sensor_data/", pattern = "tif", full.names = TRUE)

img_files <- .select_img_files("0134", 15)
for (i in img_files){
  write_path <- file.path("img_crosscal_rad/", basename(i))
  if (file.exists(write_path))
    next
  metadata <- read.delim(paste0(tools::file_path_sans_ext(i), ".tsv"))
  sensor_data <- read_caim(i)
  caim <- split_cfa(sensor_data, spec$cfa_pattern, black_level = metadata$libraw_black)
  caim <- map_spectral_bands(caim, spec$spectral_mapping)
  write_caim(caim$Blue, write_path, 16)
}

files <- dir("img_crosscal_rad/", pattern = "tif", full.names = TRUE)
aperture <- lapply(files, function(x){
  metadata <- tools::file_path_sans_ext(basename(x))
  metadata <- paste0(metadata, ".tsv")
  metadata <- file.path("sensor_data/", metadata)
  metadata <- read.delim(metadata)
  metadata$aperture
}) %>% unlist()

lapply(files, function(x){
  metadata <- tools::file_path_sans_ext(basename(x))
  metadata <- paste0(metadata, ".tsv")
  metadata <- file.path("sensor_data/", metadata)
  metadata <- read.delim(metadata)
  metadata$date_time_original
}) %>% unlist()

ig <- set_internal_geometry(my_entry$raw$dim, geometry)
radiometry <- get_radiometry_spec(geometry, "simple_method")
ff <- flat_field_image(ig$off_axis,
                       "simple_polynomial",
                       radiometry$parameters$`3.5`$Blue)

r_cal <- read_caim(files[aperture == names(radiometry$parameters)])
r_cal <- apply_flat_field(r_cal, ff)


# Sampling ----------------------------------------------------------------

query_directions <- fibonacci_directions(5)
m <- read_bin("mask_crosscal_rad/DSC_0134.tif")

ig_masked <- ig
ig_masked[!m] <- NA

fov <- extract_field_of_view(ig_masked)
query_directions <- intersect_directions_to_area(query_directions, fov)
query_coord <- coord_from_directions(query_directions, geometry) %>% add_xy(., nrow(ig))
plot(ig_masked$off_axis)
points(query_coord$x, query_coord$y)

sampling_indices <- read_points("pts_crosscal_rad/DSC_0134.csv") %>%
  add_indices() %>% rem_unselected(., !is.na(ig$off_axis))


measurements_cal <- sample_radiometry(r_cal,
                                      ig,
                                      query_directions,
                                      sampling_indices,
                                      search_radius = 1)


calibration <- lapply(files[-1], function(x) {
  measurements_uncal <- sample_radiometry(read_caim(x),
                                          ig,
                                          query_directions,
                                          sampling_indices,
                                          search_radius = 1)
  crosscalibrate_radiometry(measurements_uncal$measurements,
                            measurements_cal$measurements, degree = 3)

})

fnumber <- paste0("f/", aperture[-1])
names(calibration) <- fnumber

sink("_figures/crosscalibration_model_report.txt")

lapply(calibration, function(x) {
  summary(x$model)
})

sink()

r2 <- lapply(calibration, function(x) {
  summary(x$model)$r.squared %>% signif(2)
})

# The fitting was weak for apertures with low vignetting effect, but the
# function is quite flat
parameters <- lapply(seq_along(calibration), function(i) {
    list("Blue" = calibration[[i]]$vignetting_coef)
})
names(parameters) <- aperture[-1]

my_entry <- add_radiometry_spec(
  my_entry,
  embedded_metadata_sig = "raw",
  band_spec = "bayer",
  geometry_spec = "simple_method",
  id = "crosscalibration",
  type = "flat_field_correction",
  model = "simple_polynomial",
  parameters = parameters,
  notes = "Documented in rcaiman.registry.data source code, folder data-raw",
  contact_information = "gastonmaurodiaz@gmail.com"
)

saveRDS(my_entry, file.path(
  "_registry_entry/",
  paste0(my_entry$instrument_metadata$id, "_v3.rds")
))


# Generate figure ---------------------------------------------------------

png("_figures/vignetting_functions_crosscalibration.png", 12*1.5, 9*1.5, "cm", 8, res = 600)
par(mar = c(5, 4.5, 0.1, 0.1))

plot(c(0, 2.4), c(0.75, 1.25), type = "n",
     axes = FALSE,
     xlab = "Off-axis angle",
     ylab = "Uncorrected relative response"
)
box()
axis(1, labels = c(0,expression(pi/8), expression(pi/4),
                   expression(pi*3/8), expression(pi/2)),
     at = c(0, pi/8, pi/4, pi*3/8, pi/2))
axis(2)

for (i in seq_along(calibration)) {
  theta <- seq(0, pi/2, pi/180)

  ds <- calibration[[i]]$ds
  points(
    ds$off_axis ,
    ds$relative_response,
    lty = 1,
    lwd = 1,
    col = i,
    pch = i,
    cex = 0.8
  )

  lines(
    theta,
    calc_relative_response(
      theta,
      model = "simple_polynomial",
      parameters = calibration[[i]]$vignetting_coef,
      units = "radian"
    ),
    lwd = 2,
    col = "Black"
  )
}


x <- lapply(seq_along(fnumber), function(i) {
  u <- which.max(calibration[[i]]$ds$off_axis) - 1
  calibration[[i]]$ds$off_axis[u]
}) %>% unlist()
y <- lapply(seq_along(fnumber), function(i) {
  u <- which.max(calibration[[i]]$ds$off_axis)
  calibration[[i]]$ds$relative_response[u]
}) %>% unlist()
times <- seq_along(x) - median(seq_along(x))
times <- abs(times) * 7

u <- 0
for (i in order(x)) {
  u <- u + 1
  text(x[i],
       y[i],
       paste0(paste0(rep("·", times[u]), collapse = ""), fnumber[i]),
       pos = 4,
       cex = 0.8)
}

legend(
  "topleft",
  legend = paste0(fnumber, " (R^2=", r2,")"),
  pch = seq_along(calibration),
  col = seq_along(calibration),
  title = "Data", cex = 0.8
)

dev.off()
