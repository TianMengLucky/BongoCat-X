<div align="center">

2.2.4 : statistiques de caractères et minuteur de saisie de la session, compagnon IA fourni à configurer et activer, paramètres et grille de plugins adaptés à la taille de la fenêtre et améliorations des pages de plugins, du sélecteur de couleurs et de DirectComposition sous Windows.
  <a href="https://bongocat.pet" target="_blank">
    <img src="../resources/assets/bongocat.png" alt="BongoCat" width="249">
  </a>

  <h1>
    <a href="https://bongocat.pet" target="_blank" style="text-decoration: none; color: inherit;">BongoCat</a>
  </h1>
</div>

Les cibles de test sont désactivées par défaut pour accélérer les compilations courantes. Activez-les avec `-DBUILD_TESTING=ON`, puis recompilez avant de lancer CTest.

<!-- Description du projet + icône fusée -->
<p align="center">
 💘C/C++ × Rust × SDL3, mélangez le tout, assemblez-le ! Bong~ Bongo Cat !!!
</p>
<p align="center">
  <a href="README.en-US.md">English</a> • <a href="../README.md">简体中文</a> • <a href="README.zh-Hant.md">繁體中文</a> • <strong>Français</strong> • <a href="README.de-DE.md">Deutsch</a> • <a href="README.ko-KR.md">한국어</a> • <a href="README.pt-BR.md">Português</a> • <a href="README.ru-RU.md">Русский</a> • <a href="README.es-ES.md">Español</a> • <a href="README.id-ID.md">Bahasa Indonesia</a>
</p>
> [!NOTE]
> Ce dépôt (**BongoCat-X**) est un fork de [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat). Il n’est pas affilié à Live2D Inc. et ne contient ni Cubism SDK ni Core. Les licences Live2D doivent être respectées.

<p align="center">
  <a href="../LICENSE"><img src="https://img.shields.io/badge/AGPL--3.0-1fa669?style=flat&logo=gnu&logoColor=white"></a>
  <a href="https://github.com/TianMengLucky/BongoCat-X"><img src="https://img.shields.io/badge/C-A8B9CC?style=flat&logo=c&logoColor=white"></a>
</p>

<!-- Vidéo de démonstration -->
<div align="center" style="margin-bottom: 30px;">
  <video src="https://github.com/user-attachments/assets/75719230-9e49-4124-ae5a-8e35592c5d49
" autoplay loop style="border-radius: 8px; max-width: 800px;"></video>
</div>


> [!TIP]
> Le modèle présenté dans cette démonstration provient de [宇痕冫](https://space.bilibili.com/348616056).
>
> 🎁 Vous cherchez des modèles **gratuits** ? Nous collaborons avec des créateurs de modèles talentueux pour vous proposer une grande variété de modèles gratuits, tout en explorant continuellement de nouvelles expériences de bureau amusantes ! Rendez-vous sur notre site officiel : [bongocat.pet](https://bongocat.pet/models)


<p align="center">
  <a href="https://bongocat.pet/models">
    <img height="1080" src="https://github.com/user-attachments/assets/dedd83ca-742a-4f8c-a64a-69e659fca564" />
  </a>
</p>

<p align="center">
    <img src="https://count.getloli.com/@bongocat?name=bongocat&theme=booru-qualityhentais&padding=7&offset=0&align=top&scale=1&pixelated=1&darkmode=auto" width="400">
  </p>

## 💬 Communauté QQ

Scannez le QR code avec QQ ou recherchez le groupe **211957388** pour rejoindre la communauté **BongoCat-X**.

<p align="center"><img src="../resources/assets/qq-group.jpg" alt="BongoCat-X QQ · 211957388" width="320"></p>

## SDK Live2D et couleurs

L’importation et le téléchargement du SDK se trouvent dans Paramètres → Plugins ; la page Modèles gère les modèles. Sous Windows, activez le plugin Live2D, choisissez Télécharger le SDK Live2D, puis lisez et acceptez les deux accords officiels pour installer le Core adapté. La progression affiche le volume reçu, le total et le pourcentage ; les erreurs peuvent être réessayées dans la boîte de dialogue. Sur les autres plateformes, importez un Core compatible manuellement. Les couleurs de l’apparence et du fond uni partagent une palette qui s’ouvre en cliquant sur l’échantillon de couleur.

## 📥 Téléchargement

- GitHub Releases ([v2.2.4](https://github.com/TianMengLucky/BongoCat-X/releases/tag/v2.2.4))


---

<div align="center">

Copyright © 2026 - **BongoCat**\
Par vladelaina\
Fait avec ❤️ & ⌨️

</div>

### Présentation des options

Les préférences utilisent des cartes de groupes repliables, ouvertes par défaut pour Fenêtre, Modèle, Application et Apparence. Activez Présentation compacte dans Apparence pour réduire les espacements en conservant les descriptions. Le réglage est enregistré automatiquement.

### Plugin de statistiques des entrées

Le plugin Rust est compilé par défaut avec l’application. Activez, désactivez ou importez-le depuis la page des plugins. La page de statistiques affiche les totaux du clavier et de la souris, les graphiques par jour et par heure, le stockage local et l’export CSV. Voir [Statistiques](statistics-plugin.md). Le temps des applications actives inclut classement, totaux horaires et commande séparée. Relevés, stockage et export fonctionnent dans un thread indépendant. Le plugin ajoute l’entrée des statistiques à son activation et la retire à sa désactivation ou désinstallation.

Personnalisez les noms d’unités et les facteurs de déplacement et de défilement dans Statistiques, avec enregistrement et restauration. L’apparence utilise une palette commune pour les couleurs principale, secondaire, du texte et du fond de page.

### Plugins de rendu dynamiques

Préférences → Plugins permet d’installer, d’activer et de supprimer des moteurs de rendu locaux. Passez à un autre moteur avant de modifier un plugin actif. Live2D utilise un plugin C++ ; Inochi2D utilise le plugin Rust Inox2D avec OpenGL, Vulkan (Windows/Linux) et Metal (macOS), sans wgpu. L’importation identifie d’abord le contenu du modèle (`.model3.json`, `.inp`, `.inx`). Toutes les données JSON sont lues et écrites en Rust.

Les versions portables Windows sont distribuées sous forme d’archives ZIP. Conservez le dossier extrait `plugins` à côté de l’exécutable. Voir [Plugins de rendu](model-plugins.md) pour les paquets, les dépendances hors ligne, l’ABI, les paramètres et les limites de compatibilité.

Chaque push sur `dev` qui modifie le code, les ressources ou la configuration de compilation publie une préversion GitHub. Les modifications de documentation seules sont ignorées ; aucun déclenchement programmé. Toutes les plateformes compilent le commit poussé. La dernière version stable reste inchangée.

Les préversions nocturnes réutilisent la [page nightly](https://github.com/TianMengLucky/BongoCat-X/releases/tag/nightly), actualisent le tag et remplacent les fichiers aux noms fixes. Les plateformes en échec conservent leurs fichiers précédents.

### Linux Wayland / evdev

L’entrée evdev expérimentale sous Linux Wayland s’active avec `BONGOCAT_ENABLE_EVDEV=1`. Les périphériques sont d’abord ouverts en lecture seule comme utilisateur normal. En cas de refus, une installation protégée demande une authentification via `/usr/bin/sudo`, ouvre les périphériques puis abandonne immédiatement les privilèges et redémarre avant l’initialisation de SDL, des paramètres et des modèles. Le programme, sudo et leurs répertoires parents doivent appartenir à root et ne pas être modifiables par le groupe ou les autres utilisateurs. Les installations de développement et portables nécessitent des autorisations distinctes. Le chemin de sudo se configure avec `BONGO_CAT_SUDO_EXECUTABLE`. Un échec arrête le démarrage ; aucun droit permanent n’est modifié. Seuls les périphériques présents au démarrage sont surveillés ; tout branchement ultérieur nécessite un redémarrage. Ne pas lancer directement comme root ni rejoindre le groupe `input`. Les entrées peuvent contenir des mots de passe ; verrouillage, changement de session et masquage de l’animal ne suspendent pas la surveillance.

[SECURITY.md](../SECURITY.md#linux-input)

Préférences → Préférences → Fenêtre permet de choisir une image de fond personnalisée : PNG, JPEG, WebP, BMP ou GIF (première image). Elle remplit la fenêtre centrée, en conservant ses proportions, et exclut le fond uni. Elle est copiée dans le dossier de données de l’application et reste disponible si l’original est déplacé. Limites : 64 Mo et 16 mégapixels. Compatible avec OpenGL, Vulkan et Metal.

Live2D réutilise automatiquement les tampons de géométrie et les états de rendu répétés. Les transferts OpenGL, les lots et le cache Vulkan, les tampons partagés Metal et les masques natifs adaptatifs ne nécessitent aucun interrupteur supplémentaire. Les performances et le comportement par plateforme restent à mesurer.

Vulkan gère automatiquement la mémoire : Live2D utilise des blocs VMA de 1 MiB par défaut, ou de 16 MiB avec les grandes allocations activées. La présentation Windows par couches conserve une cible couleur. Les fenêtres masquées ou réduites libèrent les caches de lecture après la fin des opérations GPU et les recréent au besoin. Qualité et résolution dynamique restent des réglages indépendants.

Paramètres → Animal → Plage de suivi : horizontal/vertical 25–400%, désactivée par défaut, 100% inchangé. Les pattes Mver dessinées vont plus loin ; Live2D garde les limites du modèle. Regard et fenêtre inchangés

### Plusieurs touches et caractères lisibles en mode miroir

Mver (OpenGL/Vulkan/Metal) découpe séparément la main, le cadre et les caractères, puis relie les touches maintenues avec + dans une seule bulle. Le cadre et la queue se reflètent avec le modèle ; les caractères restent lisibles, aussi en mode touche unique. Configurez les trois zones, l’espacement autour de + et la limite par côté dans `hand-layout.json` à côté des images des mains, puis redémarrez. Consultez le [guide et l’exemple](hand-layout.md).

La boîte de dialogue vérifie la référence `Sound` et la présence du fichier, puis affiche un réglage de volume à côté des mouvements avec audio. Réglez de 0 à 100% par glissement ou saisie ; un double-clic rétablit 100% et 0% coupe le son. Les valeurs sont conservées par modèle et mouvement et s’appliquent au dialogue, aux raccourcis, aux mouvements aléatoires et au menu contextuel, aussi pendant la lecture. Les autres mouvements utilisant le même fichier gardent leur propre volume.

### Plugin d’objets

Le plugin d’objets indépendant est inclus par défaut. Activez-le dans Préférences → Plugins pour afficher Objets. Il prend en charge l’importation de plusieurs images et le glisser-déposer, la position, la taille, la rotation, l’opacité, les calques avant/arrière, le verrouillage, la duplication et les scènes avec OpenGL, Vulkan et Metal. Suivre la zone du modèle ne fixe pas l’objet au maillage. GIF affiche la première image ; jusqu’à 16 objets pour le compagnon principal. Voir [Plugin d’objets](items-plugin.md). `BONGO_CAT_BUILD_ITEMS=OFF` omet la bibliothèque.

Préférence mémoire du rendu: Privilégier la mémoire vidéo / Privilégier la mémoire système. Live2D Vulkan uniquement. Recharge le rendu. La mémoire vidéo favorise la vitesse ; la mémoire système réduit la pression sur la VRAM, mais peut être plus lente. Les ressources incompatibles utilisent automatiquement une alternative. Les autres moteurs allouent automatiquement ; les GPU intégrés partagent la mémoire.

### Sons du clavier

Activez Effets sonores dans Plugins. Cliquez sur une touche pour choisir un audio, survolez pour voir le nom et appuyez sur la touche physique pour jouer le son. Enregistrement automatique des affectations. Sons et objets ont leurs propres fils. WAV/MP3/Ogg Vorbis/FLAC pour le compagnon principal. Voir [Effets sonores](sounds-plugin.md) ; `BONGO_CAT_BUILD_SOUNDS=OFF` omet la bibliothèque.

La boutique Rust affiche des aperçus, des liens d’auteur et des boutons de téléchargement/désinstallation. SHA-256 est vérifié avant installation ; un état d’attente apparaît si le catalogue n’est pas publié. [Extension boutique](store-plugin.md).

La recherche automatique des mises à jour est activée par défaut dans Préférences → Application et se lance une fois en arrière-plan à chaque démarrage. Les notifications de nouvelles versions sont conservées jusqu’à l’ouverture des paramètres et proposent « Mettre à jour » en haut à droite. Toutes les notifications des paramètres peuvent être fermées manuellement.

La page des modèles regroupe les modes d’un même paquet dans une carte avec sélecteur, y compris les modes standard, clavier et manette par défaut. Plusieurs ZIP peuvent être déposés pour une importation en arrière-plan. Fermer les paramètres n’interrompt pas l’importation. Quitter arrête les imports suivants et attend la fin de la source en cours avant de libérer les threads et ressources temporaires.

Les raccourcis sont intégrés aux préférences, via Réglages / Raccourcis. La boutique est intégrée à Modèle, via Modèles locaux / Boutique. Ces deux entrées indépendantes sont retirées de la barre latérale ; chaque sous-page conserve sa position de défilement.

L’activation de Live2D sans Cubism Core affiche les deux accords officiels. Sous Windows, Accepter et télécharger récupère le SDK Native R5 via HTTPS officiel, extrait uniquement le Core dynamique compatible et active le rendu local. Annuler et continuer garde le plugin activé sans rendu. Le téléchargement peut être retenté et l’import manuel reste disponible. L’application et les paquets ne contiennent ni SDK ni Core.

DirectComposition: Composition alpha DXGI expérimentale sans lecture des pixels. Disponible pour toutes les marques de GPU avec interopérabilité du pilote. Utilise OpenGL, laisse passer tous les clics et désactive VSync, sans limiter les événements expose. En cas d’échec, revient à l’affichage OpenGL direct. Independent Flip des jeux n’est pas garanti.

L’ABI des plugins de rendu passe à v2 : mettez à jour ou recompilez ensemble l’application et les plugins Live2D/Inox2D ; les anciens plugins sont incompatibles. Live2D Vulkan propose des ressources hors écran à la demande, le redimensionnement PNG en flux avec Rust et un cache BC7 avec repli RGBA8 ; voir les [détails du rendu](live2d-vulkan-metal.md) pour les limites des plateformes.

> Le compagnon IA expérimental est compilé et fourni par défaut (`BONGO_CAT_BUILD_COMPANION=ON`), mais désactivé à l’exécution. Après configuration et activation explicite, il envoie les catégories d’événements et la personnalité au service choisi. Utilisez `OFF` pour l’exclure. Voir [la documentation](companion-plugin.md).

> Le plugin amis optionnel utilise `-DBONGO_CAT_BUILD_LAN=ON` : IPv4/IPv6, surnoms et cache temporaire des modèles. Voir [le guide LAN](lan-plugin.md).
