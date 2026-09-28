<?php
/**
 * AutoConnect OTA Update Server
 * Place in document root (e.g. fw.lnpay.eu).
 * Put .bin files in ./bin/ folder.
 */

$BIN_DIR = __DIR__ . '/bin';
$requestUri = $_SERVER['REQUEST_URI'];
$path = parse_url($requestUri, PHP_URL_PATH);
$path = trim($path, '/');
$query = parse_url($requestUri, PHP_URL_QUERY);

// Catalog request: /_catalog?op=list&path=
if ($path === '_catalog' || (isset($_GET['op']) && $_GET['op'] === 'list')) {
    header('Content-Type: application/json');
    $list = [];
    if (is_dir($BIN_DIR)) {
        $files = array_merge(
            glob($BIN_DIR . '/*.bin') ?: [],
            glob($BIN_DIR . '/*.sig') ?: []
        );
        foreach ($files as $f) {
            $name = basename($f);
            $list[] = [
                'name' => $name,
                'type' => substr($name, -4) === '.sig' ? 'sig' : 'bin',
                'date' => date('Y-m-d', filemtime($f)),
                'time' => date('H:i', filemtime($f)),
                'size' => (int) filesize($f),
            ];
        }
    }
    echo json_encode($list);
    exit;
}

// Binary or signature download: /filename.bin or /filename.bin.sig
if (preg_match('/^[a-zA-Z0-9_\-\.]+\.bin(\.sig)?$/', $path)) {
    $filePath = $BIN_DIR . '/' . basename($path);
    if (!is_file($filePath)) {
        header('HTTP/1.1 404 Not Found');
        exit;
    }
    header('Content-Type: application/octet-stream');
    header('Content-Disposition: attachment; filename="' . basename($path) . '"');
    header('Content-Length: ' . filesize($filePath));
    header('x-MD5: ' . bin2hex(md5_file($filePath, true)));
    readfile($filePath);
    exit;
}

header('HTTP/1.1 404 Not Found');
echo 'Not found';
