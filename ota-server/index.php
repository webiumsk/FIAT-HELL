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
        foreach (glob($BIN_DIR . '/*.bin') as $f) {
            $list[] = [
                'name' => basename($f),
                'type' => 'bin',
                'date' => date('Y-m-d', filemtime($f)),
                'time' => date('H:i', filemtime($f)),
                'size' => (int) filesize($f),
            ];
        }
    }
    echo json_encode($list);
    exit;
}

// Binary download: /filename.bin
if (preg_match('/^[a-zA-Z0-9_\-\.]+\.bin$/', $path)) {
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
