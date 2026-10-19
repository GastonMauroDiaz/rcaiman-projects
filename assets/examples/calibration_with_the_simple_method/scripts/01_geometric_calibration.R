# Geometric calibration with the simple method.
# Method documented in doi:10.1016/j.agrformet.2024.110020.

# Note: this is not and automatic process, use the script as a template.

require(rcaiman.registry)
rm(list = ls())

.preprocess <- function(img_files,
                        dir_name = "preprocessed_something") {

  current_imgs <- dir(dir_name, pattern = "tif")
  if (length(current_imgs) > length(img_files)) {
    stop(
      "Revise the collection",
      dir_name,
      "because there is more images in it than expected."
    )
  }
  for (i in img_files) {
    write_path <- file.path(dir_name, basename(i))
    if (file.exists(write_path))
      next
    metadata <- read.delim(paste0(tools::file_path_sans_ext(i), ".tsv"))
    sensor_data <- read_caim(i)
    cfa_pattern <- matrix(letters[1:4], nrow = 2)
    caim <- split_cfa(sensor_data, cfa_pattern, black_level = metadata$libraw_black)
    write_caim(caim, write_path, 16)
  }
}


# Folder (point collection) were manual marking is stored
point_collection <- "pts_geo"
csvs <- dir(point_collection, pattern = "csv", full.names = TRUE)

# Image preprocessing -----------------------------------------------------
source("scripts/00_helpers.R")

files <- dir("sensor_data/", pattern = "tif", full.names = TRUE)

# TIP: use the stars in rcaiman-projects to temporarily tag the first images and
# quickly consults their IDs checking "Starred only".
img_files <- .select_img_files("0501", 4)
.preprocess(img_files, "img_geo/")

if (length(csvs) < length(img_files)) {
  stop(
    "Digitize the points for geometric calibration in rcaiman-projects. ",
    "The script assume a CSV file in the folder '",
    point_collection,
    "' per each TIFF file selected for preprocessing in this script with ",
    "the helper `.select_image()`. Run the script again after digitizing."
  )
}

# Calibration -------------------------------------------------------------

calibration_points_coord <- lapply(csvs, read_points)

calibration <- calibrate_geometry(calibration_points_coord)

projection_coef <-
  fix_projection_coef(calibration$projection_coef %>% signif(5))


# Create registry ---------------------------------------------------------

my_entry <- new_registry_entry(
  "Nikon_D610.Nikkor_8mm.CIEFAP",
  body = "D610",
  body_manufacturer = "NIKON CORP",
  body_serial = "9023728",
  lens = "AF-S FISHEYE NIKKOR 8-15mm 1:3.5-4.5E ED",
  lens_manufacturer = "NIKON CORP",
  lens_serial = "210020",
  institution = "CIEFAP"
)

registry_draft <- "registry_drafts/nikon_d610_nikkor_8mm_ciefap_snippet.R"

if (!file.exists(registry_draft)) {
  stop("Use `rcaiman-projects>Registry>Build metadata rules...` and edit this ",
       "script to point to the new registry draft.")
}
source(registry_draft)

my_entry <- add_embedded_metadata_sig(
  my_entry,
  id = "raw",
  dim = c(3040, 2014),
  validated_with_libraw = "0.21.1",
  validated_with_exiftool = "13.59",
  rules = metadata_rules
)

# Use rcaiman-projects to inspect RGB band assignments and confirm the CFA
# pattern using your knowledge of the true color of objects depicted in the
# image such as the push pins.
my_entry <- add_band_spec(
  my_entry,
  embedded_metadata_sig = "raw",
  id = "bayer", cfa_pattern =  matrix(c("Red", "Green1",
                                        "Green2", "Blue"), byrow = TRUE, ncol = 2),
  spectral_mapping = list(Red = "Red", Green = "Green1", Blue = "Blue")
)

my_entry <- add_geometry_spec(
  my_entry,
  band_spec = "bayer",
  embedded_metadata_sig = "raw",
  id = "simple_method",
  model = "radial_projection",
  parameters = calibration$projection_coef,
  center_of_vision = calibration$center_of_vision %>% round(),
  normalization_radius = calibration$normalization_radius,
  normalization_angle = calibration$normalization_angle,
  max_off_axis_angle = 90,
  notes = "Documented in rcaiman.registry.data source code, folder data-raw",
  contact_information = "gastonmaurodiaz@gmail.com"
)

saveRDS(my_entry, file.path(
  "_registry_entry/",
  paste0(my_entry$instrument_metadata$id, "_v1.rds")
))


# Generate figure ---------------------------------------------------------

png("_figures/projection_function.png", 12, 9, "cm", 8, res = 600)
par(mar = c(5, 4.5, 0.1, 0.1))


plot(c(0, pi/2), range(calibration$ds$radius), type = "n",
     axes = FALSE,
     xlab = "Off-axis angle",
     ylab = "Fisheye image radius (pixel)"
)
box()
axis(1, labels = c(0,expression(pi/8), expression(pi/4),
                   expression(pi*3/8), expression(pi/2)),
     at = c(0, pi/8, pi/4, pi*3/8, pi/2))
axis(2)

.files <- lapply(seq_along(calibration_points_coord), function(i)
  rep(img_files[i], nrow(calibration_points_coord[[i]]))) %>% unlist()
.pch <- rep(NA, length(.files))
.col <- rep(NA, length(.files))

.set_symbol <- function(id = "7142", pch = 1, col = "black") {
  u <- grep(id, .files)
  .pch[u] <<- pch
  .col[u] <<- col
}

legend(
  "bottomright",
  legend = c("3 o'clock", "9 o'clock", "12 o'clock", "6 o'clock"),
  pch = 1:4,
  title = "Direction"
)
legend(
  "bottom",
  legend = c("20230824"),
  pch = 15,
  col = "black",
  title = "Session"
)

.set_symbol("0501", 1)
.set_symbol("0504", 2)
.set_symbol("0506", 3)
.set_symbol("0507", 4)

points(calibration$ds$off_axis * pi/180,
       calibration$ds$radius,
       pch = .pch,
       col = .col)

theta <- seq(0, pi/2, pi/180)
lines(
  theta,
  calc_relative_radius(theta, projection_coef, units = "radian") * calibration$normalization_radius
)

dev.off()

