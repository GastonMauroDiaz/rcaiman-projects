# Radiometric calibration with the simple method.
# Method documented in doi:10.1016/j.agrformet.2024.110020.

# Note: this is not and automatic process, use the script as a template.

require(rcaiman.registry)
rm(list = ls())

my_entry <- readRDS("_registry_entry/Nikon_D610.Nikkor_8mm.CIEFAP_v1.rds")
spec <- get_band_spec(my_entry, "raw", "bayer")

# Image preprocessing -----------------------------------------------------
source("scripts/00_helpers.R")

files <- dir("sensor_data/", pattern = "tif", full.names = TRUE)

.fun <- function(x) {
  metadata <- read.delim(paste0(tools::file_path_sans_ext(x), ".tsv"))
  sensor_data <- read_caim(x)
  caim <- split_cfa(sensor_data, spec$cfa_pattern, black_level = metadata$libraw_black)
  caim$Blue
}

# TIP: use the stars in rcaiman-projects to temporarily tag the first images and
# quickly consults their ids by clicking "Starred only".
img_files <- .select_img_files("0452", 9)
dir_name <- "img_rad_a/"
write_path <- file.path(dir_name, "rad_a.tif")
if (!file.exists(write_path)) {
  l <- lapply(img_files, .fun)
  r <- max(rast(l))
  write_caim(r, write_path, 16)
}

img_files <- .select_img_files("0461", 9)
dir_name <- "img_rad_b/"
write_path <- file.path(dir_name, "rad_b.tif")
if (!file.exists(write_path)) {
  l <- lapply(img_files, .fun)
  r <- max(rast(l))
  write_caim(r, write_path, 16)
}

img_files <- .select_img_files("0470", 9)
dir_name <- "img_rad_c/"
write_path <- file.path(dir_name, "rad_c.tif")
if (!file.exists(write_path)) {
  l <- lapply(img_files, .fun)
  r <- max(rast(l))
  write_caim(r, write_path, 16)
}


img_files <- .select_img_files("0479", 9)
dir_name <- "img_rad_d/"
write_path <- file.path(dir_name, "rad_d.tif")
if (!file.exists(write_path)) {
  l <- lapply(img_files, .fun)
  r <- max(rast(l))
  write_caim(r, write_path, 16)
}

# Calibration -------------------------------------------------------------

ig <- set_internal_geometry(my_entry$raw$dim, spec$geometry$simple_method)

.fun <- function(suffix, group) {

  img_path <- file.path(paste0("img_", suffix), paste0(suffix, ".tif"))

  if (!file.exists(img_path)) {
    stop("This script was prepared to generate an image via blending several ",
         "images with 'max()', and store the result in ", img_path, ". But, ",
         "the scripts had fail to do it, maybe because ",
         "the helper `.select_image()` were not run with a proper ID.")
  }

  r <- read_caim(img_path)

  query_path <- file.path(paste0("pts_", suffix), paste0("pts_", suffix, ".csv"))

  if (!file.exists(query_path)) {
    stop("There is not a CSV file named ", paste0("pts_", suffix, ".csv"),
         " in the folder ", paste0("pts_", suffix),
         " with manually digitized light-source centers. Use 'rcaiman-projects' ",
         "'Create empty point layer...' tool and then use the edit mode to ",
         "manually marking the image ", img_path)
  }

  query_directions <- read_points(query_path) %>%
    directions_from_coord(., spec$geometry$simple_method)

  sampling_path <- file.path(paste0("pts_", suffix), paste0(suffix, ".csv"))

  if (!file.exists(sampling_path)) {
    stop("There is not a CSV file named",  paste0(suffix, ".csv"), " in the folder ",
         paste0("pts_", suffix),
         " with a dense grid of points. Generate one with the `rcaiman-projects` tool ",
         "Point>Create grid... .")
  }

  sampling_indices <- read_points(sampling_path) %>%
    add_indices() %>% rem_unselected(., !is.na(ig$off_axis))

  samples <- sample_radiometry(
    r,
    ig,
    query_directions,
    sampling_indices,
    search_radius = 3,
    group = group
  )

  write_points(samples$points,
               file.path(paste0("pts_", suffix), paste0("samples_", suffix, ".csv")))

  samples$measurements
}

# TIP: Use rcaiman-projects to setup the groups. Collection alias can be of help.
measurements <- rbind(.fun("rad_a", "9 o'clock"),
                      .fun("rad_b", "3 o'clock"),
                      .fun("rad_c", "6 o'clock"),
                      .fun("rad_d", "12 o'clock"))


calibration <- calibrate_radiometry(measurements)


# Update registry ---------------------------------------------------------

my_entry <- add_radiometry_spec(
  my_entry,
  embedded_metadata_sig = "raw",
  band_spec = "bayer",
  geometry_spec = "simple_method",
  id = "simple_method",
  type = "flat_field_correction",
  model = "simple_polynomial",
  parameters = list("3.5" = list("Blue" = calibration$vignetting_coef)),
  notes = "Documented in rcaiman.registry.data source code, folder data-raw",
  contact_information = "gastonmaurodiaz@gmail.com"
)

saveRDS(my_entry, file.path(
  "_registry_entry/",
  paste0(my_entry$instrument_metadata$id, "_v2.rds")
))


# Generate figure ---------------------------------------------------------

png("_figures/vignetting_function.png", 12, 9, "cm", 8, res = 600)
par(mar = c(5, 4.5, 0.1, 0.1))

plot(c(0, pi/2), c(0.4, 1), type = "n",
     axes = FALSE,
     xlab = "Off-axis angle",
     ylab = "Uncorrected relative response"
)
box()
axis(1, labels = c(0,expression(pi/8), expression(pi/4),
                   expression(pi*3/8), expression(pi/2)),
     at = c(0, pi/8, pi/4, pi*3/8, pi/2))
axis(2)

points(calibration$ds$off_axis,
       calibration$ds$relative_response,
       pch = c(rep(1,9),rep(2,9),rep(3,9),rep(4,9)))


metadata <- read.delim("sensor_data/DSC_0454.tsv")
metadata$aperture

fnumber <- paste0("f/", metadata$aperture)

legend("bottomleft",
       legend = unique(calibration$ds$group),
       pch = 1:4, title = "Data")

theta <- seq(0, pi/2, pi/180)
lines(
  theta,
  calc_relative_response(
    theta,
    model = "simple_polynomial",
    parameters = calibration$vignetting_coef,
    units = "radian"
  )
)

u <- which.max(calibration$ds$off_axis)
text(1.5,
     calibration$ds$relative_response[u],
     fnumber,
     pos = 4,
     cex = 0.8)

dev.off()



