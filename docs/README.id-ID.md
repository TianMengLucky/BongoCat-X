

2.2.2: halaman dan terjemahan dikelola plugin, penggunaan memori rendering dan pemuatan gambar dioptimalkan, serta tautan pembelian di toko.
<div align="center">
  <a href="https://bongocat.pet" target="_blank">
    <img src="../resources/assets/bongocat.png" alt="BongoCat" width="249">
  </a>

  <h1>
    <a href="https://bongocat.pet" target="_blank" style="text-decoration: none; color: inherit;">BongoCat</a>
  </h1>
</div>


<!-- Deskripsi Proyek + Ikon Roket -->
<p align="center">
 💘C/C++ × Rust × SDL3, campur semuanya, satukan! Bong~ Bongo Cat!!!
</p>
<p align="center">
  <a href="README.en-US.md">English</a> • <a href="../README.md">简体中文</a> • <a href="README.zh-Hant.md">繁體中文</a> • <a href="README.fr-FR.md">Français</a> • <a href="README.de-DE.md">Deutsch</a> • <a href="README.ko-KR.md">한국어</a> • <a href="README.pt-BR.md">Português</a> • <a href="README.ru-RU.md">Русский</a> • <a href="README.es-ES.md">Español</a> • <strong>Bahasa Indonesia</strong>
</p>
> [!NOTE]
> Repositori ini (**BongoCat-X**) adalah fork [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat), tidak berafiliasi dengan Live2D Inc. dan tidak menyertakan Cubism SDK atau Core. Lisensi Live2D tetap berlaku.

<p align="center">
  <a href="../LICENSE"><img src="https://img.shields.io/badge/AGPL--3.0-1fa669?style=flat&logo=gnu&logoColor=white"></a>
  <a href="https://github.com/TianMengLucky/BongoCat-X"><img src="https://img.shields.io/badge/C-A8B9CC?style=flat&logo=c&logoColor=white"></a>
</p>


<!-- Video Demo -->
<div align="center" style="margin-bottom: 30px;">
  <video src="https://github.com/user-attachments/assets/75719230-9e49-4124-ae5a-8e35592c5d49
" autoplay loop style="border-radius: 8px; max-width: 800px;"></video>
</div>


> [!TIP]
> Model yang ditampilkan dalam demo ini berasal dari [宇痕冫](https://space.bilibili.com/348616056).
>
> 🎁 Mencari model **gratis**? Kami bekerja sama dengan kreator model berbakat untuk menghadirkan beragam model gratis, sambil terus mengeksplorasi pengalaman desktop yang lebih seru! Kunjungi situs resmi kami: [bongocat.pet](https://bongocat.pet/models)


<p align="center">
  <a href="https://bongocat.pet/models">
    <img height="1080" src="https://github.com/user-attachments/assets/dedd83ca-742a-4f8c-a64a-69e659fca564" />
  </a>
</p>

<p align="center">
    <img src="https://count.getloli.com/@bongocat?name=bongocat&theme=booru-qualityhentais&padding=7&offset=0&align=top&scale=1&pixelated=1&darkmode=auto" width="400">
  </p>

## 💬 Komunitas QQ

Pindai kode QR dengan QQ atau cari grup **211957388** untuk bergabung dengan komunitas **BongoCat-X**.

<p align="center"><img src="../resources/assets/qq-group.jpg" alt="BongoCat-X QQ · 211957388" width="320"></p>

## SDK Live2D dan warna

Impor dan unduhan SDK tersedia di Pengaturan → Plugin; halaman Model mengelola model. Di Windows, aktifkan plugin Live2D, pilih Unduh SDK Live2D, lalu baca dan setujui kedua perjanjian resmi untuk memasang Core yang sesuai. Progres menampilkan jumlah unduhan, ukuran total, dan persentase; kegagalan dapat dicoba ulang dalam dialog. Pada platform lain, impor Core yang kompatibel secara manual. Warna tampilan dan latar solid menggunakan popup palet yang sama, dibuka dengan mengeklik contoh warna.

## 📥 Unduh


---

<div align="center">

Copyright © 2026 - **BongoCat**\
Oleh vladelaina\
Dibuat dengan ❤️ & ⌨️

</div>

### Tampilan opsi preferensi

Preferensi memakai kartu grup yang dapat dilipat; Jendela, Model, Aplikasi dan Tampilan terbuka secara default. Aktifkan Tata letak ringkas pada Tampilan untuk mengurangi jarak tanpa menyembunyikan deskripsi. Pengaturan disimpan otomatis.

### Plugin statistik input

Plugin statistik Rust dibangun bersama aplikasi secara bawaan. Aktifkan, nonaktifkan, atau impor dari halaman plugin. Halaman statistik menampilkan total keyboard dan mouse, grafik harian dan per jam, penyimpanan lokal, serta ekspor CSV. Lihat [Plugin statistik](statistics-plugin.md). Waktu aplikasi aktif mencakup peringkat, total per jam dan sakelar terpisah. Pengambilan sampel, penyimpanan dan ekspor berjalan di thread tersendiri. Plugin mendaftarkan menu statistik saat diaktifkan dan menghapusnya saat dinonaktifkan atau dihapus.

Halaman Statistik mendukung nama unit serta pengali gerakan dan gulir yang dapat disimpan atau dikembalikan ke bawaan. Pengaturan tampilan memakai satu palet untuk warna utama, sekunder, teks, dan latar halaman.

### Plugin perender dinamis

Preferensi → Plugin mengelola pemasangan, pengaktifan, dan penghapusan perender lokal. Beralihlah ke perender lain sebelum mengubah plugin yang aktif. Live2D memakai plugin C++; Inochi2D memakai plugin Rust Inox2D dengan OpenGL, Vulkan (Windows/Linux), dan Metal (macOS), tanpa wgpu. Impor mengenali isi model terlebih dahulu (`.model3.json`, `.inp`, `.inx`). Seluruh pembacaan dan penulisan JSON dilakukan di Rust.

Rilis portabel Windows berbentuk arsip ZIP. Simpan folder hasil ekstraksi `plugins` di samping program. Lihat [Plugin perender](model-plugins.md) untuk pengemasan, dependensi luring, ABI, pemetaan parameter, dan batas kompatibilitas.

Setiap push ke `dev` yang mengubah kode, aset, atau konfigurasi build menerbitkan prarilis GitHub. Perubahan dokumentasi saja dilewati; tidak ada pemicu terjadwal. Semua platform membangun commit yang didorong. Prarilis tidak menggantikan rilis stabil terbaru.

Prarilis nightly memakai kembali [halaman nightly](https://github.com/TianMengLucky/BongoCat-X/releases/tag/nightly), memperbarui tag dan mengganti berkas bernama tetap. Platform yang gagal tetap menyimpan berkas sebelumnya.

### Linux Wayland / evdev

Input evdev eksperimental di Linux Wayland diaktifkan dengan `BONGOCAT_ENABLE_EVDEV=1`. Perangkat dibuka hanya untuk dibaca sebagai pengguna biasa terlebih dahulu. Jika izin ditolak, instalasi terlindungi meminta autentikasi melalui `/usr/bin/sudo`, membuka perangkat, lalu segera menurunkan hak akses dan menjalankan ulang sebelum inisialisasi SDL, pengaturan, dan model. Program, sudo, serta direktori induk harus dimiliki root dan tidak boleh ditulis oleh grup atau pengguna lain. Instalasi pengembangan dan portabel memerlukan izin perangkat yang dikelola terpisah. Jalur sudo dapat diatur melalui `BONGO_CAT_SUDO_EXECUTABLE`. Kegagalan menghentikan startup; izin permanen tidak diubah. Hanya perangkat saat startup yang dipantau; perangkat baru memerlukan restart. Jangan jalankan langsung sebagai root atau bergabung dengan grup `input`. Input mentah dapat mencakup kata sandi dan tidak berhenti saat layar terkunci, sesi berganti, atau pet disembunyikan.

[SECURITY.md](../SECURITY.md#linux-input)

Preferensi → Preferensi → Jendela menyediakan latar gambar kustom. Pilih PNG, JPEG, WebP, BMP, atau GIF (bingkai pertama). Gambar memenuhi jendela di tengah dengan rasio aspek tetap; latar gambar dan latar warna solid saling menonaktifkan. Salinan disimpan di direktori data aplikasi sehingga pemindahan berkas asli tidak memengaruhinya. Batas: 64 MB dan 16 megapiksel. Mendukung OpenGL, Vulkan, dan Metal.

Live2D otomatis menggunakan kembali buffer geometri dan status render berulang. Streaming OpenGL, unggahan berkelompok dan cache pipeline Vulkan, slot buffer bersama Metal, serta mask native adaptif tidak memerlukan sakelar tambahan. Kinerja dan perilaku platform masih perlu diukur.

Vulkan mengelola memori secara otomatis: Live2D memakai blok VMA 1 MiB secara default, atau 16 MiB jika alokasi besar diaktifkan. Presentasi berlapis Windows menyimpan satu target warna. Jendela tersembunyi atau diminimalkan melepaskan cache baca setelah GPU selesai dan membuatnya kembali saat diperlukan. Kualitas dan resolusi dinamis tetap diatur secara terpisah.

Pengaturan → Pet → Rentang mouse: horizontal/vertikal 25–400%, default nonaktif; 100% mempertahankan semula. Kaki Mver yang digambar bergerak lebih jauh; Live2D mempertahankan batas model. Pandangan dan posisi jendela tetap

### Beberapa tombol dan karakter yang terbaca dalam mode cermin

Mver (OpenGL/Vulkan/Metal) memotong tangan, bingkai gelembung, dan karakter secara terpisah, lalu menyambungkan tombol yang ditahan dengan + dalam satu gelembung. Bingkai dan ekornya mengikuti pencerminan model; karakter tetap terbaca, termasuk dalam mode satu tombol. Atur tiga area potong, jarak di sekitar +, dan batas per sisi dalam `hand-layout.json` di samping gambar tangan, lalu mulai ulang aplikasi. Lihat [panduan dan contoh](hand-layout.md).

Dialog aksi memeriksa referensi `Sound` dan keberadaan file audio, lalu menampilkan kontrol volume tersendiri di samping gerakan dengan audio. Atur 0–100% dengan menyeret atau mengetik; klik ganda mengembalikan 100% dan 0% membisukan. Nilai disimpan per model dan gerakan serta berlaku pada dialog, pintasan, gerakan acak, dan menu konteks, termasuk saat audio diputar. Gerakan lain yang memakai file sama mempertahankan volumenya sendiri.

### Plugin item

Plugin item mandiri disertakan secara default. Aktifkan di Preferensi → Plugin untuk menampilkan halaman Item. Mendukung impor banyak gambar dan seret-lepas, posisi, ukuran, rotasi, opasitas, lapisan depan/belakang, penguncian, duplikasi, dan penyimpanan adegan di OpenGL, Vulkan, dan Metal. Mengikuti area model tidak berarti menempel ke mesh. GIF memakai bingkai pertama; hingga 16 item pada pet utama. Lihat [Plugin item](items-plugin.md). `BONGO_CAT_BUILD_ITEMS=OFF` tidak menyertakan pustaka.

Preferensi memori render: Prioritaskan memori video / Prioritaskan memori sistem. Hanya Live2D Vulkan. Perubahan memuat ulang render. Memori video mengutamakan kecepatan; memori sistem mengurangi tekanan VRAM tetapi bisa lebih lambat. Sumber daya yang tidak didukung memakai alternatif otomatis. Backend lain mengalokasikan otomatis; GPU terintegrasi berbagi memori.

### Efek suara keyboard

Aktifkan Efek suara di Plugin. Klik tombol untuk memilih audio, arahkan mouse untuk melihat nama, lalu tekan tombol fisik untuk memutar suara. Penetapan tersimpan otomatis. Suara dan item memiliki thread terpisah. WAV/MP3/Ogg Vorbis/FLAC pada pet utama. Lihat [Efek suara](sounds-plugin.md); `BONGO_CAT_BUILD_SOUNDS=OFF` tidak menyertakan pustaka.

Plugin toko model Rust menampilkan pratinjau, tautan penulis, serta tombol unduh/hapus. SHA-256 diverifikasi sebelum pemasangan; status persiapan ditampilkan jika katalog belum diterbitkan. [Plugin toko](store-plugin.md).

Pemeriksaan pembaruan otomatis aktif secara default di Preferensi → Aplikasi dan berjalan sekali di latar belakang setiap kali aplikasi dimulai. Pemberitahuan versi baru disimpan hingga Pengaturan dibuka, dengan tombol Perbarui sekarang di sudut kanan atas. Semua pemberitahuan Pengaturan dapat ditutup secara manual.

Halaman model menggabungkan mode dari paket yang sama dalam satu kartu dengan pemilih mode, termasuk mode Standar, Keyboard dan Gamepad bawaan. Seret beberapa ZIP untuk diimpor berurutan di latar belakang. Menutup Pengaturan tidak menghentikan impor. Saat aplikasi ditutup, impor berikutnya dihentikan dan sumber saat ini diselesaikan sebelum thread serta sumber daya sementara dilepas.

Pintasan kini berada di Preferensi melalui Pengaturan / Pintasan. Toko berada di Model melalui Model lokal / Toko. Kedua entri sidebar terpisah dihapus; setiap subhalaman mempertahankan posisi gulirnya.

Mengaktifkan Live2D tanpa Cubism Core menampilkan kedua perjanjian resmi. Di Windows, Setuju dan unduh mengambil SDK Native R5 melalui HTTPS resmi, mengekstrak hanya Core dinamis yang sesuai dan mengaktifkan rendering lokal. Batal dan lanjutkan mempertahankan plugin aktif tanpa rendering. Unduhan dapat dicoba kembali dan impor manual tetap tersedia. Aplikasi dan paket tidak menyertakan SDK atau Core.

Windows dengan GPU AMD menyediakan presentasi DirectComposition eksperimental, default nonaktif. Komposisi alfa DXGI langsung berlaku, membatasi event expose menjadi 50 ms dan menonaktifkan VSync; kegagalan memulihkan presenter lama. Pembacaan piksel tetap digunakan; Independent Flip game tidak dijamin.

ABI plugin renderer kini v2: perbarui atau bangun ulang aplikasi dan plugin Live2D/Inox2D bersama-sama; plugin lama tidak kompatibel. Live2D Vulkan mendukung sumber daya offscreen sesuai kebutuhan, penskalaan PNG streaming Rust, serta cache BC7 dengan fallback RGBA8; batas platform dijelaskan pada [rincian rendering](live2d-vulkan-metal.md).
