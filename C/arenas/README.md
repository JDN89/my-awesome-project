# Brain dump -- cleanup later

## Waarom arenas?

[Malloc en free is traag](http://ithare.com/infographics-operation-costs-in-cpu-clock-cycles/). Zeker als je het vaak moet aanroepen voor het opslaan van geparste AST nodes (pebble compiler). Bij malloc komt er wat overhead kijken, zoals thread safety, alginemnt checks (meer hierover later, want voor arena's moet je dit manueel doen),...
Bij een arena reserveer je een chunck geheugen voor data types en je wijst van die slice altijd een deel toe voor de volgende AST node, (of andere data type). Je schuift gewoon de pointer voorwaards die dan naar het volgende stuk geheugen wijst dat nog vrij is. Bij een arena maak je alles ook in 1 keer vrij. Dus al die boekkeeping die je bij malloc hebt, heb je al niet bij arenas.

Arenas maakt ook de bookkeeping van je objecten makkelijker. Je gooit de objecten die een gelijkaardige lifetime hebben in dezelfde arena en wanneer je ze niet meer nodig hebt, kan je ze allemaal in 1 keer vrijmaken. Hierdoor moet je al niet de malloc en free rat chase doen.
Een nadeel van arena's is dat je objecten niet kan vrijmaken, maar is dit echt nodig? wanneer je een object niet langer nodig hebt, kan je het gewoon overschrijven.

Bij malloc begint er ook fragmentatie op te treden omdat je mogelijks op andere plaatsen in het geheugen de data opslaat. Bij arena zit al die data netjes bij elkaar geplakt wat zorgt voor cache locality.

##### Cache locality
In moderne pc's haalt de CPU data op in cache lines van 64 bytes. Alle data die binnen dezelfde cache line valt, kan enorm snel worden uitgelezen. Als een stuk data zich uitstrekt over meerdere cache lines, kan dit leiden tot een **cache miss**. Dat betekent dat de CPU meerdere cache lines moet ophalen om de gegevens volledig in de cache te laden, wat extra vertraging veroorzaakt. Dit kan vooral een probleem zijn als je data niet goed op cache line-grenzen is uitgelijnd.

Om dit te voorkomen, kun je ervoor zorgen dat je data netjes is uitgelijnd op de cache line-grenzen. In C zorgt de C-compiler meestal al voor uitlijning van struct-leden. Dit gebeurt door **padding** toe te voegen aan struct-leden, zodat de leden beginnen op de juiste geheugenadressen (bijvoorbeeld, een int wordt vaak uitgelijnd op een adres dat een veelvoud van 4 is). Dit helpt om cache misbruik te minimaliseren en zorgt voor efficiëntere toegang tot geheugen.

Een CPU bescikt over een L1 cache, L2, L3,..


AST nodes worden vaak sequentieel doorlopen waardoor het handig is ze dicht op elkaar te pakken zodat je maximaal gebruik kan maken van cache locality.

Hoe je data structures compacter te maken is een ander onderwerp. Zie voor een intro [Andrew Kelley Pratical Data Oriented Design (DOD)](https://www.youtube.com/watch?v=IroPQ150F6c&t=1566s) 

### Veel bla bla, show me the money
Bovenstaande uitleg klinkt mooi en ik heb er nu ook al wat over gelezen, maar ik begrijp pas echt iets door het toe te passen en de effectieve verschillen te zien!

Wat ik wil doen is de Lexer en Parser van mijn pebble project porten, AST nodes parsen en in een arena allocator opslaan. Dan de AST nodes proberen opkuisen en zien wat voor effect dat heeft.
- Eerst AST nodes in dynamic array opslaan, en malloc en free aanspreken. Dan al de node overlopen (DFS) en mooi uitprinten, om de AST te visualiseren.
- Arena allocator opslaan en geen padding voorzien.
- Arena allocator opslaan en padding voorzien voor de nodes
- Zien of er manieren zijn om de mijn AST nodes compacter te maken
- Zien wat het effect van al het bovenstaande is wanneer we L1 overschreiden en naar L2 gaan en dan naar cat_l3
- Visualiseer de padding van je Arena allocator. print de gebruikt ruimte met X padding met _ en vrije ruimte met .
- Kan ik meerdere cores gebruiken?
- Kan ik SIMD instructions gebruiken en wat is het effect op bovenstaande acties

##### Mijn CPU info
| **Cache Level**       | **Cache Size** | **Number of Instances** | **Per-Core or Shared?** | **Description**                                                                 |
|-----------------------|----------------|-------------------------|-------------------------|---------------------------------------------------------------------------------|
| **L1d (Data Cache)**  | 192 KiB        | 6                       | Per Core                | Fast, small cache for data. Each core has its own L1d cache for frequently accessed data. |
| **L1i (Instruction Cache)** | 192 KiB        | 6                       | Per Core                | Fast, small cache for instructions. Each core has its own L1i cache to store frequently used instructions. |
| **L2 Cache**           | 3 MiB          | 6                       | Per Core                | Larger than L1, but slower. Each core has its own L2 cache for additional frequently used data or instructions. |
| **L3 Cache**           | 32 MiB         | 1                       | Shared                  | The L3 cache is shared by all cores. It's larger than L1 and L2 but slower. It stores data shared between cores to reduce memory access time. |

