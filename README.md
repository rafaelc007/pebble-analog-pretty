# Pebble Analog Watchface

A clean and elegant analog watchface application for the Pebble smartwatch, featuring a modern design with a rounded square clock face, customizable markers, and smooth animated hands.


## Technical Details

### Requirements

- Pebble SDK 3.0 or higher
- libpebble library
- C compiler compatible with Pebble development

### Code Structure

The application is organized into modular components:

- **Constants**: All visual and layout parameters defined as constants for easy customization
- **Utility Functions**: Reusable helper functions for trigonometric calculations and point positioning
- **Drawing Functions**: Separated rendering logic for each visual element
- **Event Handlers**: Clean separation of UI lifecycle and update logic

## Building and Installation

### Prerequisites

1. Install the Pebble SDK
2. Set up your development environment following [Pebble's documentation](https://developer.rebble.io/developer.pebble.com/sdk/index.html)

### Build Steps

```bash
# Navigate to project directory
cd pebble-analog-watchface

# Build the project
pebble build

# Install to connected Pebble watch
pebble install --emulator basalt