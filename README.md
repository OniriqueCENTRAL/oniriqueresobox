# Onirique ResoBox

Suppresseur de résonances dynamique : il repère ce qui dépasse du spectre et le
ramène vers ce qui l'entoure, fréquence par fréquence, sans toucher à la phase.

VST3 (Windows, macOS, Linux) et Audio Unit (macOS).

---

## État du projet

| Partie | État |
|---|---|
| Moteur (STFT, détection, sidechain, stéréo / mid-side / mono) | fait, testé hors ligne |
| A/B, presets stock et utilisateur, état sauvegardé | fait, testé hors ligne |
| Plugin VST3, paramètres automatisables | compile sous Windows |
| Interface | écrite, vérifiée sur capture (`resobox_snapshot`) |
| Écoute dans un DAW | **pas encore faite** |
| Undo interne | non commencé (celui de l'hôte fonctionne) |

Les réglages par défaut, les courbes des potards et les presets stock ont été
calés sur des signaux de synthèse et par raisonnement. Ils restent à ajuster à
l'oreille sur de vraies prises.

---

## Construire

JUCE 9.0.3 est cherché dans `JUCE/`, puis dans `../PitchBox/JUCE` : les deux
plugins partagent la même copie.

```bash
cmake -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Le plugin sort dans
`build/OniriqueResoBox_artefacts/Release/VST3/ResoBox.vst3` ; à copier dans
`C:\Program Files\Common Files\VST3`.

Outil de développement, sans hôte :

```bash
cmake --build build --config Release --target resobox_snapshot
build/Release/resobox_snapshot.exe snapshot.png     # capture de l'interface
build/Release/resobox_snapshot.exe --selftest       # A/B, presets, état
```

---

## Utilisation

**En-tête — quels réglages sont chargés**

- **Presets** : `<` `>` et le menu. **SAVE** rend le nom éditable dans le menu
  lui-même, sélectionné : taper le remplace, Entrée enregistre, Échap annule.
  Le nom d'un preset stock ressort avec « (user) ». **DELETE** ne s'allume que
  sur un preset utilisateur et demande un second clic. Les presets utilisateur
  sont des fichiers dans `%APPDATA%\Onirique\ResoBox\Presets`.
- **A / B** : deux réglages côte à côte. **A > B** copie celui qu'on écoute
  dans l'autre. Les deux sont sauvegardés avec le projet.
- **DELTA** : n'écouter que ce qui est retiré. **BYPASS** : garde la latence.

Un preset ou un changement de slot ne touche ni à BYPASS, ni à DELTA, ni à la
qualité, ni à l'échelle de l'écran : ce sont des façons d'écouter et de
regarder, pas des réglages du son.

Laisser le curseur immobile deux secondes sur un contrôle affiche sa description, en français ; elle
reste tant que le curseur est sur ce contrôle, même s'il bouge, et disparaît quand il le quitte. Cela vaut aussi pour les points, les lectures et les boutons de l'îlot.

**Colonne de gauche — comment il agit**

- **SOFT / HARD** : soft arrondit l'entrée en action ; hard coupe net et deux
  fois plus fort.
- **DEPTH** : la quantité. À 0 le plugin rend l'entrée à l'identique.
- **DETAIL** : l'échelle à laquelle le spectre est lu. Bas (un tiers d'octave),
  il trouve les bosses larges et répond par des coupes larges ; haut (1/24
  d'octave), il trouve les sifflements isolés et répond par des encoches.
- **TILT** : penche toute la sensibilité vers l'aigu (+) ou le grave (−),
  autour de 1 kHz.
- **ATTACK / RELEASE** : vitesse à laquelle une coupe s'installe et se relâche.

**Écran — où il agit**

- Le corps gris est le spectre à l'entrée, le trait clair la sortie.
- La nappe rouge qui pend du haut est la réduction en cours (échelle de
  droite ; la capsule en bas à droite en change l'étendue, de 6 à 24 dB).
- La courbe blanche est la sensibilité (échelle de gauche) : au-dessus de 0 le
  plugin travaille plus fort à cet endroit, en dessous il épargne.
- Au départ : low cut, high cut et trois cloches.
  - **double-clic dans le vide** : ajoute une cloche à cet endroit (huit au
    plus) ;
  - **double-clic sur un point** : le bypasse (l'anneau reste, vide) ;
  - **glisser** : fréquence et sensibilité ; **molette** : Q ;
    **clic droit** : sensibilité et Q remis à zéro ;
  - **sélection multiple** : `Ctrl` ou `Maj` + clic, ou un cadre tracé dans
    le vide. Glisser, molette, double-clic et clic droit agissent alors sur
    toute la sélection ;
  - **Suppr** (ou Retour arrière) : retire les cloches sélectionnées. Les deux
    coupes ne se retirent pas, elles se bypassent ;
  - la sélection ouvre **l'îlot** :
    - à gauche : marche/arrêt de la bande, sa forme (**bell, low shelf, high
      shelf, plateau**) ;
    - au centre : FREQ, GAIN (la sensibilité) et Q. La valeur remplace le nom
      du potard tant que la souris est dessus ; double-clic sur le nom pour
      la taper ;
    - à droite : `<` `>` passent au point voisin, **`X` supprime le ou les
      points sélectionnés**, et le dernier menu place la bande dans l'image
      stéréo : **STEREO, LEFT, RIGHT, MID ou SIDE**, quel que soit le mode
      choisi au pied de la fenêtre. Sous une bande placée sur MID ou SIDE en
      mode stéréo (ou sur LEFT ou RIGHT en mode mid/side), le traitement se
      fait dans les termes de la bande, et LINK lâche prise : sinon il
      recopierait la coupe sur l'autre canal et le placement ne s'entendrait
      pas. Une bande placée porte sa lettre (L, R, M, S) au-dessus de son
      point ;
    - avec plusieurs points il reste là et agit sur tous : GAIN décale chacun
      du même nombre de dB, Q du même rapport ; seul FREQ se grise.
- En bas à gauche de l'écran : la fréquence et la sensibilité sous le curseur
  (et, curseur ailleurs, le rappel des gestes).
- Chaque valeur en capsule sous un potard se clique pour être tapée au clavier
  (`3.3k`, `-4.5`…).

**Pied — ce qu'on fait du résultat**

- **MIX**, **OUT**.
- **AUTO GAIN** : rend le niveau que les coupes ont enlevé. Il compare le
  niveau avant et après traitement sur ~0,4 s, les deux canaux ensemble, et ne
  rend jamais plus de 12 dB. DELTA reste ce qui est retiré, sans ce gain.
- **STEREO / MID-SIDE / MONO** : en mono la détection se fait sur la somme et
  les deux côtés reçoivent exactement la même coupe.
- **LINK** : à 100 % les deux canaux reçoivent la coupe la plus profonde des
  deux ; à 0 % chacun est traité seul. **BAL** : épargne un côté (L/R, ou
  mid/side selon le mode).
- **SIDECHAIN** : la détection se fait sur l'entrée latérale, que le plugin
  déclare à l'hôte et qui est active d'office. Il reste à y envoyer un signal
  depuis l'hôte ; une entrée latérale silencieuse ne coupe rien.
- **QUALITY** : longueur de la trame — Normal 2048 échantillons (43 ms à
  48 kHz), High 4096 (85 ms), Ultra 8192 (171 ms).
- **LOW LATENCY** : trame de 1024 (21 ms), quelle que soit la qualité.

La latence est déclarée à l'hôte.

**Ce que la qualité change, et ce qu'elle ne change pas.** La détection lit
toujours la même longueur de signal (4096 échantillons à 48 kHz), quelle que
soit la trame de traitement : ce qui est trouvé, et de combien c'est baissé,
est le même dans les quatre modes — à moins de 0,5 dB près sur les signaux de
test, y compris une note grave aux harmoniques serrés. L'écran montre lui
aussi le même spectre partout. Ce qui change :

- **la finesse avec laquelle une coupe est appliquée** : une trame courte ne
  peut pas tailler plus étroit qu'environ 50 Hz, donc une encoche fine devient
  plus large en low latency (même profondeur, plus de voisinage emporté) ;
- **le retard de la détection sur le son** : en low latency elle regarde
  forcément en arrière, d'environ 30 ms ; en normal, 20 ms ; en high et ultra
  elle est centrée sur ce qui est traité. Une résonance tenue n'en souffre
  pas, une attaque très brève est rattrapée un peu tard.

Dans tous les modes, rien n'est détecté sous environ 47 Hz, faute de voisinage
à gauche pour comparer.

---

## Comment c'est construit

```
Source/dsp/                  sans dépendance, testable en ligne de commande
  Fft                        FFT radix-2
  ResonanceSuppressor        tout ce qui décide du son
Source/PluginProcessor       paramètres, bus, état, slots A/B, presets
Source/PluginEditor          la fenêtre
Source/ui/SpectrumDisplay    l'écran et ses points
Source/ui/Controls           potard à valeur éditable, champ numérique
Source/ui/Theme              l'identité Onirique, reprise de PitchBox
```

### Ce qui en fait autre chose qu'un compresseur multibande

Un compresseur à bandes baisse ce qui passe un seuil. Ici, une résonance est
définie comme un endroit où le spectre dépasse **ce qu'il y a des deux côtés** :

1. le niveau de chaque bin est moyenné sur ~25 ms, puis lissé à l'échelle
   DETAIL ;
2. chaque point est comparé à la moyenne d'un tronçon de spectre à sa gauche
   et d'un autre à sa droite, pris à distance pour ne pas contenir le pic
   lui-même ;
3. c'est **le plus haut des deux côtés** qui sert de référence. Un pic dépasse
   les deux ; une pente, un plateau ou le bord d'un signal filtré n'en dépasse
   qu'un, et n'est pas touché ;
4. le dépassement, pondéré par la courbe de sensibilité, donne la réduction,
   qui a donc la forme du pic qu'elle corrige ;
5. liaison stéréo, attaque et relâchement par bin ; gain appliqué au module
   seul, puis resynthèse (Hann, recouvrement 75 %).

Toutes les largeurs sont en fractions d'octave et la référence est le spectre
du signal lui-même : le résultat ne dépend ni du niveau d'entrée, ni de la
longueur de la trame de traitement : la détection a sa propre trame, toujours la
même, lue dans un historique de l'entrée.

### Ce que les tests vérifient

`test_suppressor` (moteur) :

- depth 0, bypass et mix 0 rendent l'entrée retardée de la latence déclarée
  (erreur < −100 dB), pour les quatre trames ;
- mêmes échantillons quel que soit le découpage en blocs (64, 333, 512, 4096) ;
- une résonance de 12 dB perd plus de 3 dB au réglage par défaut, le spectre
  à côté et un bruit sans résonance moins de 0,5 dB ;
- même réduction à 0,5 dB près dans les quatre trames, sur une résonance dans
  l'aigu comme sur un harmonique trop fort d'une note à 150 Hz ;
- un plateau de 14 dB et le bord d'un signal coupé à 8 kHz ne sont pas
  touchés (< 1 dB) ;
- cloches, coupes et tilt agissent dans le bon sens ; delta + sortie = entrée ;
  link, balance, mono, mid/side et sidechain.

`resobox_snapshot --selftest` (processeur) : slots A/B et copie, état
sauvegardé avec les deux slots, presets stock, sauvegarde / rechargement /
suppression d'un preset utilisateur.

---

## Licence

JUCE 9 est sous double licence AGPLv3 ou commerciale : une distribution payante
à code fermé demande une licence JUCE commerciale.
