"""Disable auto-reset for upload - use manual BOOT+RESET instead."""
Import("env")

board_config = env.BoardConfig()
upload_cfg = dict(board_config.get("upload", {}))
upload_cfg["use_1200bps_touch"] = False
upload_cfg["wait_for_upload_port"] = False  # don't wait for port change
board_config.update("upload", upload_cfg)
