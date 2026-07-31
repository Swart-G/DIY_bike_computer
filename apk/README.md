# Android companion APK 2.2.0

`DIY-Bike-Computer-2.2.0-release.apk` is the production release build:

- package: `com.diybikecomputer.companion`;
- version: `2.2.0` (`versionCode 3`);
- minimum Android: 8.0 (API 26);
- signature schemes: APK Signature Scheme v2, v3 and a verified v4 `.idsig` sidecar;
- signing certificate: RSA 4096, SHA-256 fingerprint
  `3B:02:73:F9:41:62:2A:C9:43:47:D4:D7:60:49:DF:1B:52:94:94:A3:0E:7D:20:28:BB:52:FF:9C:02:FA:B7:58`.

Verify and install from this directory:

```bash
sha256sum -c SHA256SUMS
apksigner verify --verbose --print-certs \
  --v4-signature-file DIY-Bike-Computer-2.2.0-release.apk.idsig \
  DIY-Bike-Computer-2.2.0-release.apk
keytool -printcert -file diy-bike-computer-release-cert.pem
adb install -r DIY-Bike-Computer-2.2.0-release.apk
```

The certificate is embedded in the APK; the PEM copy is provided for independent
verification. The private update key is intentionally not in the repository.

An older debug APK has a different signature and must be uninstalled once before this
release can be installed. Future 2.2.0+ release builds signed by this same key can update
in place. Android may still show its normal confirmation for applications installed
outside a trusted store.
