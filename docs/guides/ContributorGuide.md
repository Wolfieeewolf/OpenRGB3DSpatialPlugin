# Contributing to OpenRGB 3D Spatial Plugin

Thank you for your interest in contributing to the OpenRGB 3D Spatial Plugin! This document provides guidelines for contributing to the project.

## Code of Conduct

This project follows the OpenRGB community standards. Be respectful, constructive, and collaborative.

## Getting Started

### Prerequisites
- OpenRGB development environment set up
- Qt 5.15 or Qt 6.8+ installed
- Git for version control
- Familiarity with C++ and Qt framework

### Development Setup

1. **Fork and Clone**
   ```bash
   git clone https://gitlab.com/YOUR_USERNAME/OpenRGB3DSpatialPlugin.git
   cd OpenRGB3DSpatialPlugin
   git submodule update --init --recursive
   ```

2. **Build the Plugin**
   - Follow instructions in README.md for your platform
   - Ensure the plugin loads in OpenRGB successfully

3. **Create a Feature Branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

4. **Pre-submit review**
   - Scan your changes for `auto`, range-based loops, Qt usage outside `ui/`, or other violations listed below.
   - Ensure builds succeed on at least one supported toolchain before opening a merge request.

## Reference Documentation

Always consult the upstream OpenRGB resources when adding features or SDK touch points:

- `../guides/GridSDK.md` - spatial API surface exposed by this plugin
- `../../CONTRIBUTING.md` (root) - quick reference pointing back here
- `../OpenRGBSDK.md` - network SDK protocol details (kept in `D:\MCP`)
- `../RGBControllerAPI.md` - controller abstraction expectations

Staying aligned with these documents keeps the plugin compatible with the broader OpenRGB ecosystem.

## Coding Standards

This plugin **strictly follows** [OpenRGB Contributing Guidelines](https://gitlab.com/CalcProgrammer1/OpenRGB/-/blob/master/CONTRIBUTING.md).

### Style Requirements

#### Functional Style
- **Use C-style indexed for loops** (not range-based)
  ```cpp
  // Correct
  for(unsigned int i = 0; i < items.size(); i++)

  // Incorrect
  for(auto& item : items)
  ```

- **No `auto` keyword** - use explicit types
  ```cpp
  // Correct
  std::vector<LEDPosition3D> positions = GenerateLEDPositions();

  // Incorrect
  auto positions = GenerateLEDPositions();
  ```

- **Limit std library types**
  - Use C arrays for fixed-length containers
  - Use `std::vector` for variable-length containers
  - Use `std::string` for variable-length strings
  - Prefer basic types (`int`, `float`) over typedef'd (`uint32_t`, etc.)

- **No Qt types outside UI code**
  - Qt is only allowed in `ui/` folder
  - Use `std::vector` not `QVector`
  - Use `std::string` not `QString` (except in UI)

- **Use LogManager for logging**
  ```cpp
  // Correct
  LOG_INFO("[3D Spatial] Device added");
  LOG_ERROR("[3D Spatial] Failed to load: %s", error.c_str());

  // Incorrect
  qDebug() << "Device added";
  printf("Failed to load\n");
  ```

#### Non-Functional Style

- **4 spaces per tab** (not tabs, spaces)
- **Braces on own lines**
  ```cpp
  // Correct
  if(condition)
  {
      doSomething();
  }

  // Incorrect
  if(condition) {
      doSomething();
  }
  ```

- **No space between keyword and parenthesis**
  ```cpp
  // Correct
  if(condition)
  for(int i = 0; i < count; i++)

  // Incorrect
  if (condition)
  for (int i = 0; i < count; i++)
  ```

- **Align assignments when appropriate**
  ```cpp
  unsigned int            width           = current_zone->matrix_map->width;
  unsigned int            height          = current_zone->matrix_map->height;
  float                   led_spacing     = 1.0f;
  ```

- **Use snake_case for variables, CamelCase for functions/classes**
  ```cpp
  int led_count = 10;                    // Variable
  void UpdateLEDColors();                // Function
  class SpatialEffects;                  // Class
  ```

- **Header comment blocks**
  ```cpp
  /*---------------------------------------------------------*\
  | FileName.cpp                                              |
  |                                                           |
  |   Brief description                                       |
  |                                                           |
  |   Date: 2025-09-24                                        |
  |                                                           |
  |   This file is part of the OpenRGB project                |
  |   SPDX-License-Identifier: GPL-2.0-only                   |
  \*---------------------------------------------------------*/
  ```

### Code Quality

- **No debug code** - Remove all `qDebug()`, `printf()`, `std::cout`
- **No TODO/FIXME comments** - Complete features before committing
- **Minimal comments** - Code should be self-documenting
- **Error handling** - Always check return values and handle errors
- **Thread safety** - Use mutexes for shared data in multi-threaded code

## Contribution Workflow

### 1. Adding a New Spatial Effect

1. **Define the effect** in `SpatialEffects.h`:
   ```cpp
   enum SpatialEffectType
   {
       // ... existing effects ...
       SPATIAL_EFFECT_YOUR_NEW_EFFECT  = 25,
   };
   ```

2. **Implement calculation** in `SpatialEffects.cpp`:
   ```cpp
   RGBColor SpatialEffects::CalculateYourNewEffect(Vector3D pos, float time_offset)
   {
       // Your effect algorithm
       // pos = LED position in 3D space
       // time_offset = animation time

       float value = sin((pos.x + time_offset) / 10.0f);
       return LerpColor(params.color_start, params.color_end, value);
   }
   ```

3. **Add case to switch** in `UpdateLEDColors()`:
   ```cpp
   case SPATIAL_EFFECT_YOUR_NEW_EFFECT:
       color = CalculateYourNewEffect(led_pos.world_position, time_offset);
       break;
   ```

4. **Update UI** in `OpenRGB3DSpatialTab.cpp`:
   ```cpp
   effect_type_combo->addItem("Your New Effect");
   ```

5. **Test thoroughly** on multiple devices
6. **Update documentation** in README.md

### 2. Adding New Features

1. **Discuss first** - Open an issue to discuss major features
2. **Keep scope limited** - One feature per merge request
3. **Follow existing patterns** - Look at similar code for consistency
4. **Test cross-platform** - Verify on Windows/Linux/macOS if possible
5. **Document changes** - Update README.md and CHANGELOG.md

### 3. Bug Fixes

1. **Describe the bug** - What's broken and how to reproduce
2. **Root cause** - Explain what was causing the issue
3. **The fix** - Describe your solution
4. **Testing** - How you verified the fix works
5. **No scope creep** - Don't combine fixes with new features

## Submitting Changes

### Commit Messages

Follow this format:
```
Brief summary (50 chars or less)

Detailed description:
- What changed and why
- Technical details if needed
- Related issues

 Generated with [Claude Code](https://claude.ai/code)

Co-Authored-By: Your Name <your.email@example.com>
```

### Merge Request Process

1. **Update your branch**
   ```bash
   git fetch origin
   git rebase origin/master
   ```

2. **Ensure quality**
   - Code follows OpenRGB style guidelines
   - No debug code or TODOs
   - All files have proper headers
   - LogManager used for all logging

3. **Create Merge Request**
   - Clear title describing the change
   - Detailed description with context
   - Link related issues
   - Mark as draft if not ready for review

4. **Code Review**
   - Address reviewer feedback promptly
   - Keep discussions focused and respectful
   - Rebase if requested (not merge)

5. **Testing**
   - Test on your platform
   - Wait for CI/CD pipeline results
   - Verify no regressions

## Project Structure

```
OpenRGB3DSpatialPlugin/
 OpenRGB/                      # OpenRGB SDK (submodule - don't modify)
 ui/                           # Qt UI components
    OpenRGB3DSpatialTab.*     # Main plugin tab
    LEDViewport3D.*           # 3D viewport widget
    CustomControllerDialog.*  # Custom controller dialog
 ControllerLayout3D.*          # LED position generation
 SpatialEffects.*              # Effect calculation engine
 VirtualController3D.*         # Custom controller system
 LEDPosition3D.h               # 3D position structures
 OpenRGB3DSpatialPlugin.*      # Plugin interface
```

## Testing

### Manual Testing Checklist
- [ ] Plugin loads in OpenRGB without errors
- [ ] All effects work correctly
- [ ] Device positioning works (drag & drop)
- [ ] Custom controllers create successfully
- [ ] Save/load layouts work correctly
- [ ] Auto-load on startup works
- [ ] No crashes or memory leaks
- [ ] UI is responsive

### Platform Testing
- [ ] Windows (Qt 5.15, MSVC 2019)
- [ ] Windows (Qt 6.8, MSVC 2022)
- [ ] Linux (Qt 5.15, GCC)
- [ ] macOS (Qt 5.15, Clang)

## Documentation

### When to Update Docs
- **README.md**: New features, installation changes, usage updates
- **CHANGELOG.md**: All user-facing changes
- **CONTRIBUTING.md**: Process or guideline changes
- **Code comments**: Complex algorithms only (keep minimal)

### Documentation Style
- Clear and concise language
- Step-by-step instructions
- Code examples where helpful
- User-focused (not developer-focused for README)

## Getting Help

- **Issues**: https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/issues
- **OpenRGB Discord**: https://discord.gg/AQwjJPY
- **OpenRGB GitLab**: https://gitlab.com/CalcProgrammer1/OpenRGB

## License

By contributing, you agree that your contributions will be licensed under GPL-2.0-only, consistent with the OpenRGB project.

## Recognition

Contributors will be recognized in:
- Git commit history
- CHANGELOG.md release notes
- Project credits

Thank you for contributing to the OpenRGB 3D Spatial Plugin! 
