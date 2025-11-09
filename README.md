# PurrLauncher

A modern Minecraft launcher written in C++ with support for custom modpacks, authentication, and plugin system.

## Features

- **Custom Authentication**: Supports custom authentication servers
- **Modpack Management**: Automatic downloading and updating of custom modpacks
- **Plugin System**: Extensible plugin architecture with DLL support
- **Java Management**: Automatic Java 17 download and installation
- **Cross-Platform**: Built with modern C++20 standards
- **Configuration Management**: JSON-based configuration with validation

## Requirements

- Windows 10/11 (64-bit)
- CMake 3.16 or higher
- Visual Studio 2019/2022 with C++20 support
- vcpkg package manager (recommended)

## Dependencies

- **cURL**: For HTTP requests and file downloads
- **nlohmann/json**: For JSON parsing and configuration
- **Windows API**: For system integration and plugin loading

## Building

### Using vcpkg (Recommended)

1. Install vcpkg and integrate with Visual Studio:
```cmd
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

2. Install dependencies:
```cmd
vcpkg install curl nlohmann-json
```

3. Build the project:
```cmd
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### Manual Build

1. Ensure cURL and nlohmann/json are installed on your system
2. Configure CMake with appropriate paths to dependencies
3. Build using your preferred generator

## Configuration

Create a `config.json` file in the application directory:

```json
{
    "api_url": "https://your-api-server.com",
    "debug": true,
    "java_downloaded": false,
    "java_path": "",
    "log_file": "launcher.log",
    "max_ram": "4G",
    "pack_manifest_url": "https://your-api-server.com/manifest",
    "pack_url": "https://your-api-server.com/pack",
    "pack_version": "1.0.0"
}
```

### Configuration Options

- `api_url`: Your custom authentication server URL
- `pack_url`: URL for modpack downloads
- `pack_manifest_url`: URL for modpack manifest/version information
- `max_ram`: Maximum RAM allocation (e.g., "4G", "8G")
- `debug`: Enable debug logging
- `java_path`: Path to Java executable (auto-detected if empty)

## API Server Setup

This launcher requires a custom API server that provides:

1. **Authentication endpoint**: `/api/auth/validate`
   - Parameters: `token`, `hwid`
   - Returns: User information and validation status

2. **Yggdrasil compatibility**: `/authserver/authenticate`
   - Mojang-compatible authentication endpoint

3. **Modpack endpoints**:
   - Manifest endpoint for version checking
   - Download endpoint for modpack files

## Plugin Development

Create plugins as Windows DLLs with the following exports:

```cpp
extern "C" __declspec(dllexport) void Initialize() {
    // Plugin initialization code
}

extern "C" __declspec(dllexport) void Cleanup() {
    // Plugin cleanup code
}
```

Place plugin DLLs in the `plugins/` directory.

## Usage

1. Run the executable
2. Enter your authentication token when prompted
3. The launcher will:
   - Download Java if needed
   - Update the modpack
   - Launch Minecraft with proper authentication

## Customization

### Changing URLs

Update the following files to use your own servers:

- `config.json`: Main configuration file
- `config.cpp`: Default URL values
- `main.cpp`: Fallback URL values
- `plugin_downloader.cpp`: Plugin download URLs

### Java Distribution

To use a different Java distribution, modify `java.cpp`:

```cpp
const std::string javaUrl = "https://your-java-distribution-url.zip";
```

## Security Considerations

- Never commit actual API URLs or tokens to version control
- Use environment variables or external configuration for sensitive data
- Implement proper token validation on your API server
- Consider using HTTPS for all API endpoints

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project is open source. Please ensure compliance with all dependencies' licenses.

## Support

For support and questions:
- Check the Issues section
- Review the configuration documentation
- Ensure your API server implements the required endpoints

## Version History

- v2.4.104: Latest release with improved plugin system and configuration management
