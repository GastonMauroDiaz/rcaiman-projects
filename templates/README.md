# rcaiman project

This directory follows the rcaiman project convention.

The application automatically discovers images, point sets, and other resources based on folder names and file names.

Additional folders and files may be created as needed. However, renaming standard folders may prevent automatic discovery of project resources.

## Design philosophy

An rcaiman project is primarily a directory structure.

The filesystem is considered the source of truth.

The application interprets project contents but does not require a database or proprietary storage format.

Project resources are discovered through documented conventions whenever possible.