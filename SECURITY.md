# Security and Setup Guide

## Before Publishing to GitHub

### 1. Remove Sensitive Data

This repository has been cleaned of sensitive information, but please verify:

- ✅ API URLs replaced with examples (`https://your-api-server.com`)
- ✅ Authentication tokens removed
- ✅ Personal server URLs sanitized
- ✅ `.gitignore` updated to protect future sensitive data

### 2. Files to Review Before First Run

1. **config.json** - Contains example configuration, update with your values
2. **config.example.json** - Template for other users
3. **API_DOCUMENTATION.md** - Documentation for implementing your API server

### 3. Required Changes for Production Use

Replace placeholder URLs in these files:

1. **config.json** - Main configuration
2. **config.cpp** - Default values (lines ~99, ~133)  
3. **main.cpp** - Default values (lines ~113-117)
4. **plugin_downloader.cpp** - Plugin download URL

## Setting Up Your Own Instance

### 1. Server Requirements

You need to implement a server with these endpoints:
- `GET /api/auth/validate` - Token validation
- `POST /authserver/authenticate` - Yggdrasil-compatible auth
- `GET /manifest` - Modpack version information  
- `GET /modpack` - Modpack download

### 2. Configuration Steps

1. Clone this repository
2. Copy `config.example.json` to `config.json`
3. Update all URLs in `config.json` to point to your server
4. Build the project using CMake and vcpkg
5. Run and enter your authentication token

### 3. API Server Implementation

See `API_DOCUMENTATION.md` for detailed API specification.

Example minimal server structure:
```
/api/auth/validate      - User validation
/authserver/authenticate - Mojang-compatible auth
/manifest              - Modpack info
/pack              - Modpack download
/plugins/             - Plugin downloads (optional)
```

## Security Best Practices

### For Developers

1. **Never commit sensitive data**
   - Use environment variables for secrets
   - Keep `config.json` in `.gitignore`
   - Use the example config as template

2. **API Security**
   - Implement proper token validation
   - Use HTTPS in production
   - Rate limit authentication attempts
   - Validate all input parameters

3. **Token Management**
   - Use secure random token generation
   - Implement token expiration
   - Store tokens securely (hashed)
   - Allow token revocation

### For Users

1. **Configuration Security**
   - Keep your `config.json` private
   - Don't share authentication tokens
   - Use strong authentication on your server

2. **Network Security**
   - Use HTTPS for all API endpoints
   - Consider VPN for additional security
   - Monitor server logs for suspicious activity

## Deployment Checklist

- [ ] All placeholder URLs replaced with real endpoints
- [ ] API server implemented and tested
- [ ] Authentication system working
- [ ] Modpack hosting configured
- [ ] SSL certificates installed
- [ ] Rate limiting implemented
- [ ] Logging and monitoring set up
- [ ] Backup procedures in place

## Troubleshooting

### Common Issues

1. **"Failed to authenticate"**
   - Check API server is running
   - Verify token is valid
   - Check network connectivity
   - Review server logs

2. **"Failed to download modpack"**
   - Check modpack URL in config
   - Verify server has files
   - Check disk space
   - Review download permissions

3. **"Java not found"**
   - Will auto-download on first run
   - Check internet connectivity
   - Verify disk space for Java installation

### Debug Mode

Enable debug mode in `config.json`:
```json
{
    "debug": true
}
```

This will create detailed logs in `launcher.log`.

## Contributing

When contributing to this project:

1. Never commit real API URLs or tokens
2. Test with example configuration
3. Update documentation for new features
4. Follow existing code style and patterns
5. Ensure cross-platform compatibility where possible

## License Compliance

This project uses these open source libraries:
- cURL (MIT-like license)
- nlohmann/json (MIT license)
- CMake build system

Ensure compliance with all dependency licenses in your distribution.
