#!/bin/bash
#
# Test APK Generator
# Creates a minimal test APK/ZIP file for testing purposes
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMP_DIR="$SCRIPT_DIR/temp"

create_minimal_apk() {
    local output_path="$1"
    local temp_apk_dir="$TEMP_DIR/apk_content"
    
    mkdir -p "$temp_apk_dir"
    
    mkdir -p "$temp_apk_dir/META-INF"
    mkdir -p "$temp_apk_dir/res/layout"
    mkdir -p "$temp_apk_dir/res/values"
    mkdir -p "$temp_apk_dir/lib/armeabi-v7a"
    mkdir -p "$temp_apk_dir/lib/arm64-v8a"
    mkdir -p "$temp_apk_dir/assets"
    
    cat > "$temp_apk_dir/META-INF/MANIFEST.MF" << 'EOF'
Manifest-Version: 1.0
Created-By: APKCheck Test Generator

EOF
    
    cat > "$temp_apk_dir/AndroidManifest.xml" << 'EOF'
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.test.app"
    android:versionCode="1"
    android:versionName="1.0" >
    
    <application
        android:icon="@drawable/icon"
        android:label="Test App" >
        <activity android:name=".MainActivity" >
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
EOF
    
    cat > "$temp_apk_dir/classes.dex" << 'EOF'
dex
035
00000000
EOF
    
    cat > "$temp_apk_dir/res/layout/main.xml" << 'EOF'
<?xml version="1.0" encoding="utf-8"?>
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:orientation="vertical" >
    
    <TextView
        android:layout_width="wrap_content"
        android:layout_height="wrap_content"
        android:text="Hello World!" />
    
</LinearLayout>
EOF
    
    cat > "$temp_apk_dir/res/values/strings.xml" << 'EOF'
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="app_name">Test App</string>
</resources>
EOF
    
    cat > "$temp_apk_dir/assets/test.txt" << 'EOF'
This is a test asset file.
Used for APKCheck testing purposes.
EOF
    
    echo "libtest.so placeholder" > "$temp_apk_dir/lib/armeabi-v7a/libtest.so"
    echo "libtest.so placeholder" > "$temp_apk_dir/lib/arm64-v8a/libtest.so"
    
    cat > "$temp_apk_dir/resources.arsc" << 'EOF'
ARSC
placeholder
EOF
    
    cd "$temp_apk_dir"
    
    if command -v zip &> /dev/null; then
        zip -r -0 "$output_path" . > /dev/null 2>&1
        echo "Created test APK at: $output_path"
        echo "Size: $(stat -c%s "$output_path") bytes"
    else
        echo "WARNING: 'zip' command not found. Creating tar archive instead."
        tar -cf "$output_path.tar" .
        echo "Created test TAR at: $output_path.tar"
    fi
    
    cd - > /dev/null
}

create_signed_apk_example() {
    local keystore="$1"
    local unsigned_apk="$2"
    local signed_apk="$3"
    local alias="$4"
    local password="$5"
    local apkcheck_bin="$6"
    
    echo ""
    echo "=== Creating Signed APK Example ==="
    echo ""
    
    echo "Step 1: Create keystore..."
    "$apkcheck_bin" keygen \
        -k "$keystore" \
        -a "$alias" \
        -p "$password" \
        -P "$password" \
        -b 2048 \
        -d 36500 \
        -c "Test Developer" \
        -u "Test Team" \
        -O "Test Company" \
        -C "US" \
        -f
    
    echo "Step 2: Sign APK..."
    "$apkcheck_bin" sign \
        -k "$keystore" \
        -a "$alias" \
        -p "$password" \
        -P "$password" \
        -o "$signed_apk" \
        -n \
        -f \
        "$unsigned_apk"
    
    echo "Step 3: Verify signature..."
    "$apkcheck_bin" verify -V "$signed_apk"
}

main() {
    local output_apk="$1"
    local apkcheck_bin="$2"
    
    if [ -z "$output_apk" ]; then
        output_apk="$SCRIPT_DIR/test_app.apk"
    fi
    
    if [ -z "$apkcheck_bin" ]; then
        local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
        local build_dir="$(cd "$script_dir/../build/bin" && pwd 2>/dev/null)" || true
        apkcheck_bin="$build_dir/apkcheck"
    fi
    
    mkdir -p "$TEMP_DIR"
    
    echo ""
    echo "=== APKCheck Test APK Generator ==="
    echo ""
    echo "Output: $output_apk"
    echo "APKCheck: $apkcheck_bin"
    echo ""
    
    echo "Creating minimal test APK structure..."
    create_minimal_apk "$output_apk"
    
    if [ -f "$apkcheck_bin" ] && command -v zip &> /dev/null; then
        local keystore="$SCRIPT_DIR/test.keystore"
        local signed_apk="$SCRIPT_DIR/test_app_signed.apk"
        local alias="testkey"
        local password="TestPassword123!"
        
        create_signed_apk_example "$keystore" "$output_apk" "$signed_apk" "$alias" "$password" "$apkcheck_bin"
        
        echo ""
        echo "=== Generated Files ==="
        echo "  Unsigned APK: $output_apk"
        echo "  Keystore:     $keystore"
        echo "  Signed APK:   $signed_apk"
        echo ""
    fi
    
    rm -rf "$TEMP_DIR"
}

main "$@"
