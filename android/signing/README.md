# Android release signing

Release APKs use one persistent private key. The private keystore and its passwords
must never be committed. Configure a build with either:

- a private `android/keystore.properties` based on
  `android/keystore.properties.example`; or
- `BIKE_RELEASE_STORE_FILE`, `BIKE_RELEASE_STORE_PASSWORD`,
  `BIKE_RELEASE_KEY_ALIAS` and `BIKE_RELEASE_KEY_PASSWORD` environment variables.

The public certificate in this directory lets a user verify an APK without
exposing the update key:

```bash
apksigner verify --verbose --print-certs \
  --v4-signature-file APK_FILE.idsig APK_FILE
keytool -printcert -file signing/diy-bike-computer-release-cert.pem
```

Keep an offline backup of the private key and password. Android accepts an
in-place update only when the package name and signing identity match. A debug
build installed before this release uses a different identity and therefore
requires a one-time uninstall before installing the release APK.

Signing proves the APK identity and integrity. Android may still show its normal
confirmation for apps installed outside a trusted store; an APK certificate
cannot and should not bypass that platform warning.
