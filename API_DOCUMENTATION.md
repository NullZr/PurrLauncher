# API Documentation for PurrLauncher

This document describes the API endpoints that your server needs to implement to work with PurrLauncher.

## Authentication API

### Validate Token Endpoint

**URL:** `GET /api/auth/validate`

**Parameters:**
- `token` (string): User authentication token
- `hwid` (string): Hardware ID of the client machine

**Response:**
```json
{
    "username": "player_name",
    "registered": true,
    "valid": true
}
```

**Error Response:**
```json
{
    "error": "Invalid token",
    "valid": false
}
```

### Yggdrasil Authentication Endpoint

**URL:** `POST /authserver/authenticate`

**Request Body:**
```json
{
    "username": "player_name",
    "password": "auth_token",
    "clientToken": "client_uuid",
    "requestUser": true
}
```

**Response:**
```json
{
    "accessToken": "access_token_here",
    "clientToken": "client_uuid",
    "availableProfiles": [
        {
            "id": "player_uuid",
            "name": "player_name"
        }
    ],
    "selectedProfile": {
        "id": "player_uuid", 
        "name": "player_name"
    }
}
```

## Modpack API

### Manifest Endpoint

**URL:** `GET /manifest`

**Response:**
```json
{
    "version": "1.2.3",
    "minecraft_version": "1.20.1",
    "forge_version": "47.2.0",
    "files": [
        {
            "path": "mods/example-mod.jar",
            "hash": "sha256_hash_here",
            "size": 1234567,
            "url": "https://your-server.com/files/example-mod.jar"
        }
    ]
}
```

### Modpack Download Endpoint

**URL:** `GET /modpack`

**Response:** ZIP file containing the modpack

**Headers:**
- `Content-Type: application/zip`
- `Content-Length: file_size`

## Implementation Notes

1. **HWID Generation**: The launcher generates hardware ID based on system components
2. **Token Validation**: Implement proper token validation with expiration
3. **File Integrity**: Use SHA256 hashes for file verification
4. **HTTPS**: All endpoints should use HTTPS in production
5. **Rate Limiting**: Implement rate limiting to prevent abuse

## Security Considerations

- Validate all input parameters
- Use secure token generation
- Implement proper session management
- Log authentication attempts
- Consider implementing IP-based restrictions

## Example Implementation

Here's a basic example structure for your API server:

```python
from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route('/api/auth/validate')
def validate_token():
    token = request.args.get('token')
    hwid = request.args.get('hwid')
    
    # Implement your validation logic here
    if is_valid_token(token, hwid):
        return jsonify({
            'username': get_username(token),
            'registered': is_hwid_registered(hwid),
            'valid': True
        })
    else:
        return jsonify({'valid': False, 'error': 'Invalid token'}), 401

@app.route('/authserver/authenticate', methods=['POST'])
def authenticate():
    data = request.get_json()
    username = data.get('username')
    password = data.get('password')
    
    # Implement Yggdrasil-compatible authentication
    if authenticate_user(username, password):
        return jsonify({
            'accessToken': generate_access_token(),
            'clientToken': data.get('clientToken'),
            'availableProfiles': get_user_profiles(username),
            'selectedProfile': get_selected_profile(username)
        })
    else:
        return jsonify({'error': 'Invalid credentials'}), 401
```
