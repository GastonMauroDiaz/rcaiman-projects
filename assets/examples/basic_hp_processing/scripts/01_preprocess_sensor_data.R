# Produces preprocessed images from sensor data in TIFF format.
#
# The regitry entry was filled with calibration data from Lang et al. (2010) and
# the R script TRV_OBS_NikonCPX4500_FC-E8.config.R started on 09.11.2025 by Mait
# Lang, Tartu Observatory.
#
# The result is images with digital values linearly related with radiation.
#
# ## Bibliography
# Lang, M., Kuusk, A., Mõttus, M., Rautiainen, M., & Nilson, T. (2010). Canopy
# gap fraction estimation from digital hemispherical images using sky radiance
# models and a linear conversion method. Agricultural and Forest Meteorology,
# 150(1), Article 1. https://doi.org/10.1016/j.agrformet.2009.08.001


# Nikon CoolPix 4500 that is operated in service mode to store sensor raw data.
# For each image take there will be instant image and a file with sensor data.
# Some preprocessing is necessary to correct file numbers and convert sensor raw
# data to common NEF format.
# Rawdata have to extracted first using e.g.   Coolpix RAW to Coolpix NEF
# converter by paul69, v0.09, 2003-04-18 The EXIF data then identifies the
# camera as Nikon E5700.

require(rcaiman.registry)
entry_Nikon_Coolpix4500.FCE8.TO <- new_registry_entry(
  "Nikon_Coolpix4500.FCE8.TO",
  body = "CoolPix 4500",
  body_manufacturer = "NIKON CORP",
  lens_manufacturer = "NIKON CORP",
  auxiliary_lens = "FE-C8 ﬁsh-eye converter",
  auxiliary_lens_manufacturer = "NIKON CORP",
  institution = "Tartu Observatory, Tõravere, Estonia"
)

source("registry_drafts/cpx4500_snippet.R")

entry_Nikon_Coolpix4500.FCE8.TO <- add_embedded_metadata_sig(
  entry_Nikon_Coolpix4500.FCE8.TO,
  id = "exif_01",
  dim = c(2288, 1708)/2,
  validated_with_libraw = "0.21.1",
  validated_with_exiftool = "13.59",
  rules = metadata_rules
)

entry_Nikon_Coolpix4500.FCE8.TO <- add_geometry_spec(
  entry_Nikon_Coolpix4500.FCE8.TO,
  embedded_metadata_sig = "exif_01",
  id = "to_method",
  model = "radial_projection",
  parameters = c(0.69456186, -0.01624598, -0.01294943),
  zenith_col_row = round(c(1162+0, 881+0)/2,0),
  horizon_radius = 756/2,
  is_horizon_circle_clipped = FALSE,
  max_zenith_angle = 94.8,
  notes = "The Centre was estimated using dcraw -4 -D Full size: 2288 x 1708.
    Measured diameter was adapted to rcaiman rules.",
  contact_information = "gastonmaurodiaz@gmail.com"
)

entry_Nikon_Coolpix4500.FCE8.TO <- add_radiometry_spec(
  entry_Nikon_Coolpix4500.FCE8.TO,
  embedded_metadata_sig = "exif_01",
  geometry_spec = "to_method",
  id = "spectral_bands",
  type = "interpretive_constraint",
  cfa_pattern = matrix(c("Green", "Yellow",
                         "Magenta", "Cyan"), byrow = TRUE, ncol = 2),
  spectral_mapping = list(Red = "(Yellow + Magenta)/2",
                          Green = "Green",
                          Blue = "Cyan"),
  offset_value = list("100" = 0),
  contact_information = "lang@to.ee"
)

entry_Nikon_Coolpix4500.FCE8.TO <- add_radiometry_spec(
  entry_Nikon_Coolpix4500.FCE8.TO,
  embedded_metadata_sig = "exif_01",
  geometry_spec = "to_method",
  id = "to_method",
  type = "flat_field_correction",
  model = "ff_simple_polynomial",
  parameters = list("5.3" = c(0.0638, -0.101)),
  notes = "Aperture do not influence vignetting. Aperture value reported in Lang et al. (2010)'s fig. 5. ",
  contact_information = "lang@to.ee"
)

export_registry_snapshot(entry_Nikon_Coolpix4500.FCE8.TO,
                         "exif_01",
                         "to_method",
                         "spectral_bands",
                         "to_method",
                         "preprocessed_images/entry_Nikon_Coolpix4500_FCE8_TO_raw.yaml")

spec <- get_geometry_spec(entry_Nikon_Coolpix4500.FCE8.TO,  "exif_01", "to_method")

zenith_col_row <- spec$zenith_col_row
horizon_radius <- spec$horizon_radius
cfa_pattern <- spec$radiometry$spectral_bands$cfa_pattern
spectral_mapping <- spec$radiometry$spectral_bands$spectral_mapping
black_level <- spec$radiometry$spectral_bands$offset_value

rspec <- get_radiometry_spec(spec, "to_method")

files <- dir("sensor_data/", pattern = "tif", full.names = TRUE)

z <- zenith_image(horizon_radius * 2, spec$parameters)

for (i in seq_along(files)) {
  sensor_data <- read_caim(files[i])

  if (i == 1) cfa_labels <- label_cfa(sensor_data)
  stop()

  caim <- split_cfa(sensor_data, cfa_labels, cfa_pattern, black_level$`100`)
  caim <- map_spectral_bands(caim, spectral_mapping)

  upper_left <- zenith_col_row - horizon_radius
  side <- horizon_radius * 2
  caim <- crop_caim(caim, upper_left, side, side)

  ff <- ff_simple_polynomial(z, rspec$parameters$`5.3`)
  caim <- apply_flat_field(caim, c(ff, ff, ff))

  write_caim(caim, paste0("preprocessed_images/", basename(files[i])), 16)
}


