# Calibration of circular hemispherical images

This example provides a template for calibrating an imaging system that produces
circular hemispherical images, using the simple method.

It combines interactive work in **rcaiman-projects** with R scripts from the
rcaiman suite. The application is used to organize files, inspect images,
digitize points, create masks, build metadata rules, and keep the workflow
traceable. The scientific calibration steps are performed in R.

This is one practical way to organize calibration work, not the only correct
project structure. Adapt it to your imaging system and experimental setup.

## Before you begin

Read the documentation of the **rcaiman.registry** R package for instructions on
the experiments and image acquisition required for calibration. The scripts
include comments, tips, and checks to guide you, but these do not replace the
documentation or a careful review of the code.

**The scripts are templates, not a fully automated pipeline.** Their image IDs,
instrument details, file paths, and other settings belong to the example. Replace
them with values appropriate for your project, and work through the scripts in
stages, alternating between R and rcaiman-projects as needed.

## Suggested workflow

### 1. Import the RAW files

Import the RAW photographs acquired for your calibration experiments into the
project.

### 2. Extract sensor data and metadata

Run **Project > Extract raw data and metadata** to generate sensor TIFFs and
metadata sidecars in `sensor_data/`.

### 3. Set up your R session

If you use RStudio, create an RStudio project in this project folder. Run the
scripts with RStudio as a project so that their relative paths resolve correctly.

### 4. Perform geometric calibration

Use `scripts/01_geometric_calibration.R` as a template. Geometric calibration is
the required foundation for the subsequent calibration steps.

First, replace the example image IDs with those of your geometric calibration
photographs and run the preprocessing stage. Once the preprocessed images are
available, digitize the calibration points in rcaiman-projects and save them in
the corresponding point collection. Then return to R to fit the geometric model.

Before creating the registry entry, replace the example instrument information
and review the metadata, band, and geometry settings. Use
**Registry > Build metadata rules...** to create the metadata rules for your
system, and update the script to use the resulting draft.

Figure generation is optional. Adapt the `.set_symbol()` calls and legends to
your image IDs and experimental groups before running that section.

### 5. Perform radiometric calibration

Continue with `scripts/02_radiometric_calibration.R`. Radiometric calibration is
strongly recommended, but unlike geometric calibration, it is optional in this
workflow. Expect substantially more work, particularly during image acquisition.
Consult the rcaiman.registry documentation for the experimental procedure; the
script provides additional guidance in context.

Adapt the image IDs and settings, preprocess the images, and use rcaiman-projects
to prepare the required point layers and sampling grids before fitting the model
in R.

This step adds a vignetting correction for a particular aperture to the registry
entry. To calibrate additional apertures, either repeat the procedure at each
aperture or use radiometric cross-calibration as described below.

Figure generation is optional and may require adjustments to match your data.

### 6. Extend the calibration to other apertures

Use `scripts/03_radiometric_crosscalibration.R` for radiometric
cross-calibration. This step uses a series of photographs taken under stable
lighting conditions, changing only the aperture. One aperture must already be
calibrated to serve as the reference.

Adapt the script to your image series and reference aperture. The script prepares
the images, extracts the measurements, fits the vignetting models, and adds the
results to the registry entry.

Figure generation is optional and may require adjustments to match your data.

### 7. Use the resulting registry entry

The scripts save successive versions of the registry entry as `.rds` files in
`_registry_entry/`. Load the version you need with `readRDS()`. The resulting
entry can also be exported as a registry snapshot.
