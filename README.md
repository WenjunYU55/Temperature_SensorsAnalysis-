# Temperature Sensor Characterisation and Analysis

This repository contains the experimental setup, raw data, analysis code, and results for ELEC70101 focused on temperature sensor characterisation and comparison.

## Sensors Investigated
- TMP36 (analog temperature sensor)
- PT1000 (RTD)
- MLX90614 (infrared temperature sensor)
- MAX31855 (thermocouple interface)

## Objectives
- Design a safe and suitable test set up for temperature measurements
- Design read out electronics
- Lay out a good data acquisition strategy
- Characterise sensor performance including sensitivity, linearity, accuracy, and stability
- Compare measured performance against datasheets and reference measurements

## Repository Structure
The repository is organised as follows:
- `data/` – raw and processed experimental data
- `code/` – MATLAB scripts and STM32 embedded code used for data acquisition and analysis
- `docs/` – relevant documents e.g. schematic, pcb, figures
- `logs/` – experimental notes and observations

## Notes
All plots and analysis are generated from experimentally measured data. Raw data is preserved and not modified.
