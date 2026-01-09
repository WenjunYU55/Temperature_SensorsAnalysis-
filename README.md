# Temperature Sensor Characterisation and Analysis

This repository contains the experimental set-up details, data acquisition and analysis scripts, and experimental results for ELEC70101 (Group 2) focused on temperature sensor characterisation and comparison.

## Sensors Investigated
- TMP36 (analogue temperature sensor)
- PT1000 (RTD)
- MLX90614 (infrared temperature sensor)
- MAX31855 (k-type thermocouple)

## Objectives
- Design a safe and suitable test set up for temperature measurements
- Design read out electronics
- Lay out a good data acquisition strategy
- Characterise sensor performance including sensitivity, linearity, accuracy, and stability
- Compare measured performance against datasheets and reference measurements

## Repository Structure
The repository is organised as follows:
- `code/` – MATLAB scripts and STM32 embedded code used for data acquisition, processing, and analysis
- `docs/` – relevant documents e.g. schematic, pcb, figures
- `experimental set-up/` – design files related to the experimental design and set-up

## Notes
All plots and analysis are generated from experimentally measured data. Raw data is preserved and not modified.
