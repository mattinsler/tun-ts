{
  "targets": [
    {
      "target_name": "tun",
      "sources": [
        "src/macos.cc",
        "src/linux.cc"
      ],
      "include_dirs": ["<!(node -e \"require('nan')\")"]
    }
  ]
}