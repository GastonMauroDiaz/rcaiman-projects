# rcaiman project

This directory follows the rcaiman project convention.

The application automatically discovers images, masks, point sets, and other resources based on folder names and file names.

Additional folders and files may be created as needed. However, renaming standard folders may prevent automatic discovery of project resources.

## Standard folders

### raw

Original camera files as acquired in the field.

### unprocessed_image

Images generated from RAW files without scientific preprocessing.

### preprocessed_image

Scientifically calibrated images ready for analysis.

Each image may be accompanied by metadata describing the preprocessing workflow and registry entries used.

### skymask

Candidate sky pixels identified by segmentation procedures.

### points

Sky samples represented as directional observations.

### predictedsky

Predicted sky radiance models and associated outputs.

### ratio

Intermediate products derived from observed and predicted radiance.

### results

Final products and derived variables.

## Design philosophy

An rcaiman project is primarily a directory structure.

The filesystem is considered the source of truth.

The application interprets project contents but does not require a database or proprietary storage format.

Project resources are discovered through documented conventions whenever possible.