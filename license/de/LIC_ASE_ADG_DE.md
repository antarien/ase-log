# Antares Simulation Engine und ADG – Lizenzabkommen

**Deutsche Übersetzung der englischen Fassung; Kennungen und Regelungsinhalt bleiben unverändert.**

**Kennung: ASE-ADG-LIC-2026-10-16-1.0**  
**Ausgabedatum: 16. Oktober 2026**  
**Vertragliche Lizenzgeberin: ANTARES SIMULACAO LTDA — CNPJ 69.363.366/0001-21**

Dies ist eine eigene Lizenz. Sie ist weder die MIT-Lizenz noch die Apache-Lizenz oder eine uneingeschränkte Open-Source-Lizenz. Öffentlicher Quellcodezugang beseitigt die nachfolgenden Beitrags- und kommerziellen Bedingungen nicht. Allein für das Herunterladen, Forken, Nutzen oder Entwickeln öffentlicher Komponenten nach diesem Vertrag ist kein Backend-Abonnement erforderlich.

## 1. Parteien, Befugnis und Geltungsbereich

1.1 „Lizenzgeberin“ bezeichnet ANTARES SIMULACAO LTDA mit Sitz in Rua Milton de Brito Firmeza 180, Icaraí, Caucaia, Ceará, CEP 61624-365, Brasilien. „Sie“ bezeichnet die natürliche oder juristische Person, die diesen Vertrag annimmt. Wer für eine juristische Person handelt, muss diese benennen und befugt sein, sie zu verpflichten. Verbundene Unternehmen sind nicht automatisch derselbe Lizenznehmer.

1.2 Die Lizenzgeberin schließt Verträge im eigenen Namen. Jan Ohlmann behält die von ihm privat gehaltenen Rechte und ist Gründer und Konzeptautor. Seine Nennung in Urheberrechtsvermerken oder als Freigabestelle des Projekts macht ihn nicht persönlich für die Kundenleistungen der Lizenzgeberin verantwortlich. Deren Befugnis zur Lizenzierung seiner privaten Materialien beruht auf einem gesonderten Vertrag; dieses Dokument überträgt diese Materialien nicht selbst auf die Gesellschaft.

1.3 Dieser Vertrag gilt für Materialien, die ausdrücklich unter seiner Kennung freigegeben werden. Eine Lizenzdatei in einem übergeordneten Repository ersetzt keine eigenständige Lizenz in einem Unterrepository oder einer Drittanbieterabhängigkeit. Die Lizenzgeberin gewährt nur Rechte, die sie wirksam innehat. Gesetzliche Rechte und unabhängig unter anderen Lizenzen erworbene Rechte werden nicht entzogen.

1.4 „ADG“ bezeichnet die Projektgemeinschaft Antares Developer Group; allein ihre Bezeichnung begründet keine eigenständige juristische Person. ADG-Mitglieder sind die nach der Projektvereinbarung PJV.md, Kennung ASE-ADG-PJV-2026-10-16-1.0, aufgenommenen natürlichen und juristischen Personen. Bloße öffentliche Nutzung erfordert oder begründet keine Mitgliedschaft.

1.5 Das Projekt betrifft die Antares Simulation Engine („Engine“) und ihr Ökosystem. Antares Open World ist ein Projekt innerhalb dieses Ökosystems. Die erlaubte Nutzung ist nicht auf dieses bestimmte Spiel beschränkt.

## 2. Komponenten, Erweiterungen und Ihre eigenen Produkte

2.1 „Komponenten“ umfassen den Engine-Code und zugehörige Module, Plugins, Schnittstellendefinitionen, Build-Materialien und Dokumentation, die ausdrücklich von diesem Vertrag erfasst sind. Eigenständig lizenzierte Materialien behalten ihre Lizenzen und Hinweise.

2.2 Eine „Ökosystem-Erweiterung“ ist eine Änderung einer Komponente oder ein neues Modul beziehungsweise Plugin, das Engine-Funktionalität erweitert, anpasst oder bereitstellt, einschließlich über die Engine-API für einen Client. Dazu gehören ein ausschließlich für Ihr eigenes Produkt geschriebenes Modul und eine neue Implementierung ohne kopierten Engine-Quellcode. Die Pflicht zur Einbringung einer solchen Erweiterung ist eine vertragliche Bedingung; sie hängt nicht davon ab, ob die Erweiterung urheberrechtlich ein abgeleitetes Werk ist.

2.3 Ihr „Produkt“ ist Ihr eigenes Spiel, Ihre Client-Anwendung oder Ihr Werkzeug. Dessen eigenständiger Quellcode, Oberfläche, Spielinhalte, Assets und produktspezifische Logik verbleiben bei Ihnen oder den jeweiligen Rechteinhabern. Der bloße Aufruf einer API macht nicht das gesamte Produkt zu einem Beitrag. Umgekehrt befreit die Unterbringung eines Engine-Moduls in einem Client-Repository oder einer ausführbaren Datei dieses Modul nicht von der Pflicht. Der funktionale Zweck und nicht die Bezeichnung oder Verpackung bestimmt die Grenze. Eine eigenständige bereits vorhandene Bibliothek wird nicht allein durch ihre Nutzung automatisch eingebracht; erforderliche Berechtigungen müssen dennoch vorliegen.

2.4 Beispielsweise bleibt ein Landschaftseditor das Produkt seines Entwicklers. Ein dafür entwickeltes, über die Engine-API angebundenes Erosionsmodul ist eine Ökosystem-Erweiterung und muss eingebracht werden. Der eigenständige Quellcode des Editors muss nicht offengelegt werden.

2.5 Ein „Beitrag“ ist ein tatsächlicher Entwicklungsstand, der über das Beitragsverfahren des Projekts eingereicht wird, einschließlich initialer, unvollständiger und zwischenzeitlicher Versionen. Der Begriff umfasst nicht automatisch jedes Werk, das ein Beitragender künftig schaffen könnte.

## 3. Annahme und Freigabeinformationen

3.1 Dieser Vertrag wird mit den betreffenden Komponenten angeboten. Sie müssen vor seiner Annahme Gelegenheit erhalten, ihn zu lesen. Sie nehmen ihn durch ausdrückliche elektronische Bestätigung oder, soweit rechtlich wirksam, durch bewusste Ausübung der hier angebotenen Befugnisse nach Erhalt der Bedingungen an. Ein bloßer Download gilt nicht als abschließender Nachweis dafür, dass jede vertragliche Zahlungspflicht bewusst angenommen wurde. Unabhängig von GitHub oder zwingendem Recht gewährte Zugangs- oder Plattformrechte bleiben unberührt.

3.2 Vor privatem Zugang oder Ihrem ersten Beitrag werden im Projektverfahren Ihre Identität, eine gegebenenfalls vertretene juristische Person, die LIC- und PJV-Fassungen und das freigegebene Repository beziehungsweise die Aufgabe dokumentiert. Ein autorisierter Initial-Commit und Push nach Bereitstellung der Bedingungen kann die Annahme dokumentieren, ohne für jeden Commit einen Papiervertrag zu erfordern. Der Name eines Commit-Autors allein beweist keine Befugnis, Code eines Arbeitgebers zu lizenzieren.

3.3 Die kommerziellen Bedingungen in den Abschnitten 9–11 müssen vor der Monetarisierung bereitgestellt und angenommen werden. Eine Bestätigung unter Nennung dieser Lizenzfassung genügt; die Aushandlung eines zusätzlichen Beteiligungssatzes ist nicht erforderlich. Ein öffentlicher Download begründet kein PaaS-Abonnement.

3.4 Der jeweilige Repository- oder Komponentenhinweis bezeichnet die öffentliche oder beschränkte Freigabe, etwaige gesonderte Hinweise und bei beschränktem Zugang die Berechtigung, die Empfänger und Dauer bestimmt. Eine autorisierte öffentliche Freigabe unter diesem Vertrag ist eine „öffentliche Komponente“. Ein Datenleck, eine fehlerhafte Zugangseinstellung oder ein unbefugter Upload ist keine öffentliche Freigabe. Eine spätere Änderung der Repository-Sichtbarkeit widerruft keine bereits eingeräumten Rechte an rechtmäßig erhaltenen öffentlichen Komponenten.

## 4. Öffentliche Nutzungsbefugnisse

4.1 Vorbehaltlich dieses Vertrags gewährt Ihnen die Lizenzgeberin eine weltweite, nicht ausschließliche Lizenz zum Herunterladen, Klonen, Forken, Kopieren, Untersuchen, Ausführen, Ändern und Entwickeln öffentlicher Komponenten sowie zu deren Verwendung in Ihren eigenen Produkten. Diese Befugnisse hängen nicht vom Erwerb oder Fortbestand eines Backend-Abonnements ab. Bei Einhaltung dieses Vertrags bleiben sie nach Kündigung oder Downgrade bestehen.

4.2 Sie dürfen öffentliche Komponenten einschließlich geänderter Versionen weitergeben, wobei dieser Vertrag, zutreffende Urheberrechts- und Namensnennungshinweise sowie einschlägige Dritthinweise erhalten bleiben müssen. Empfänger müssen die geltenden Lizenzbedingungen erhalten. Sie dürfen private Komponenten nicht als öffentlich freigegeben darstellen oder Bedingungen auferlegen, die Empfänger an der Ausübung der vom tatsächlichen Lizenzgeber gewährten Befugnisse hindern. Ihr Fork ersetzt die Projektbeitragspflichten aus Abschnitt 6 nicht.

4.3 Öffentliche Entwicklung kann nichtkommerziell oder kommerziell erfolgen. Produktmeldungen und Umsatzbeteiligungen gelten wie nachfolgend bestimmt. Für jede vertragsgemäße Nutzung öffentlicher Komponenten ist keine individuelle Erlaubnis erforderlich, abgesehen von der vorherigen Projektfreigabe für neue einzubringende Module und Plugins. Die öffentliche Weiterverwendung durch eine Person ohne ADG-Mitgliedschaft gewährt keinen Zugang zu privaten ADG-Repositorys.

4.4 Bei Einhaltung der Bedingungen werden öffentliche Befugnisse für die Dauer der betreffenden Schutzrechte eingeräumt. Die Einstellung des Vertriebs oder die Beendigung der Mitgliedschaft hebt sie nicht für sich genommen auf. Dieser Vertrag enthält keine Zusage künftiger Versionen, kostenlosen Hostings, Supports oder der Annahme eines Beitrags.

## 5. Beschränkte und Enterprise-Befugnisse

5.1 Eine beschränkte Komponente darf nur im Rahmen ihrer dokumentierten Berechtigung verwendet werden. Eine Community-Berechtigung kann aus einer genehmigten Projektrolle entstehen; eine Enterprise-Berechtigung aus dem jeweiligen Kundenvertrag. Keine davon gewährt Zugang zu jedem privaten Repository.

5.2 Der berechtigte Kunde darf seine ordnungsgemäß autorisierten Mitarbeiter und Auftragnehmer innerhalb dieser Berechtigung für sich tätig werden lassen, vorbehaltlich der Vertraulichkeit und Lizenzeinhaltung. Dies ist keine eigenständige Lizenz an diese Personen, verbundene Unternehmen, Publisher oder Kunden des Kunden. Eine gesondert ausgewiesene persönliche Community-Rolle darf nicht zur Umgehung der Beschränkungen eines Arbeitgebers oder Kunden verwendet werden.

5.3 Eine Enterprise-Berechtigung erlaubt die bezeichnete Einsichtnahme, Entwicklung und Anpassung privaten Codes während ihrer genehmigten Laufzeit. Der Besitz einer heruntergeladenen Kopie verlängert diese Laufzeit nicht. Ein gewöhnliches Enterprise-Abonnement erlaubt weder die Weitergabe privaten Quellcodes noch dessen Offenlegung durch einen öffentlichen Fork, die Gewährung eigenständiger Engine-Entwicklungsrechte an ein anderes Unternehmen oder den Verkauf einer Engine-Unterlizenz.

5.4 Dieser Vertrag gewährt kein Recht zum Betrieb eines dedizierten Engine-Servers oder eigenständigen Engine-PaaS-Dienstes. Solche Betriebsrechte bedürfen einer ausdrücklichen gesonderten Einräumung. Erlaubte clientseitige Ausführung und Offline-Simulationsparität sind nicht allein deshalb ein dedizierter Serverbetrieb. Eigenständige Rechte aus einer anderen Lizenz bleiben unberührt.

## 6. Freigabe und fortlaufende Einbringung

6.1 Bevor Sie eine neue Ökosystem-Erweiterung beginnen, legen Sie deren Zweck, Umfang und die vorgeschlagene Modul- oder Plugin-Abgrenzung Jan Ohlmann oder einer von ihm ausdrücklich autorisierten Projektstelle zur Freigabe vor. Arbeiten an bestehenden Komponenten müssen dem dokumentierten Aufgaben- und Prüfverfahren folgen. Die Freigabe berücksichtigt den Simulationszweck der Engine, veröffentlichte Projektstandards und mitgeteilte ethische Beschränkungen. Es besteht kein Recht, beliebige Inhalte in das Projekt zu erzwingen.

6.2 Nach der Freigabe muss die Erweiterung in ihrem zugewiesenen Projekt-Repository initialisiert und in die vorgegebene Superrepository-/Unterrepository-Struktur eingebunden werden. Autorisierte Zugangsdaten werden über das Zugangsverfahren bereitgestellt. Die Entwicklung muss dort ab ihrem initialen Stand sichtbar sein, nicht erst bei Fertigstellung oder kommerzieller Veröffentlichung.

6.3 Tatsächliche Entwicklungsstände müssen im Rahmen des Entwicklungsablaufs fortlaufend eingecheckt und gepusht werden. Lokale Bearbeitung bleibt möglich; die Zurückhaltung des Entwicklungsprozesses in einem internen Fork bis zur endgültigen Lieferung ist jedoch nicht vertragsgemäß. Firmen-Forks und Spiegelungen sind innerhalb ihrer Zugangsberechtigungen erlaubt, ersetzen aber das maßgebliche Beitrags-Repository nicht. Diese Klausel begründet keine unangekündigte tägliche oder stündliche Frist.

6.4 Beiträge müssen den geltenden dokumentierten Architektur-, Coding-, Schnittstellen-, Versions- und Dokumentationsstandards entsprechen und bearbeitbaren Quellcode, README, erforderliche Build- und Integrationsinformationen sowie Lizenzhinweise enthalten. Maßgeblich ist der für die genehmigte Arbeit mitgeteilte Standard; ein späterer Standard schreibt frühere Pflichten nicht stillschweigend um.

6.5 Firmen-, Mitarbeiter-, Privatpersonen- und Community-Beiträge unterliegen denselben Regeln. Interne Nutzung, fehlender Verkauf und ausschließliche Einbindung in Ihr eigenes Produkt befreien eine Ökosystem-Erweiterung nicht. Produkte selbst bleiben nach Abschnitt 2 ausgenommen.

6.6 Stellen Sie Beiträge den für das betreffende Projekt-Repository berechtigten Empfängern zur Verfügung. Dies erfordert keine Veröffentlichung beschränkten Engine-Codes im Internet. Jan, die Lizenzgeberin und die für die betreffende Komponente berechtigten ADG-Mitglieder müssen die eingebrachte Entwicklung nach Abschnitt 7 erhalten und nutzen können.

6.7 Checken Sie keine Passwörter, Produktionsschlüssel, personenbezogenen Kundendaten oder Geheimnisse Dritter ein. Stellen Sie stattdessen sichere Konfigurationsinformationen bereit. Sicherheitsrelevante Arbeiten müssen den autorisierten geschützten Projektkanal nutzen. Ein Plattformausfall ist zu melden; angesammelte Arbeit ist nach Wiederherstellung des Zugangs zu synchronisieren. Absichtliche Zurückhaltung ist kein Ausfall.

6.8 Die Integration in den Hauptzweig ist eine gesonderte Prüfentscheidung. Die Beitragsrechte gelten für tatsächlich autorisierte Einreichungen einschließlich Zwischenständen, ohne auf einen Merge zu warten. Freigabe oder Merge können eine unbefugte Einreichung von Drittmaterial nicht rechtmäßig machen.

## 7. Beitragsrechte und fortbestehende Verfügbarkeit

7.1 Für jeden Beitrag, zu dessen Einreichung Sie befugt sind, gewähren Sie nicht ausschließliche, weltweite Rechte für seine geltende Schutzdauer, ihn für das Engine-Ökosystem und vertragsgemäße Produkte zu kopieren, auszuführen, zu untersuchen, zu ändern, zu verbinden, weiterzuentwickeln, zu vervielfältigen, innerhalb des genehmigten Freigabeumfangs zu vertreiben, kommerziell zu verwerten und unterzulizenzieren. Allein wegen der Ausübung dieser Rechte ist keine gesonderte Beitragsvergütung geschuldet. Arbeitsvergütung und gesondert vereinbarte Vergütungen bleiben unberührt.

7.2 Die Einräumung begünstigt ausdrücklich Jan Ohlmann persönlich, die Lizenzgeberin und die für die Komponente berechtigten ADG-Mitglieder. Sie können diesen Vorteil annehmen und im gesetzlich zulässigen Umfang durchsetzen. Jans Begünstigung ist vom fortdauernden Betrieb der Lizenzgeberin unabhängig. Die Gewährung dieser Beitragsrechte macht ihn nicht zum Leistungserbringer gegenüber Ihrem Kunden.

7.3 Jan darf Beiträge in seiner privaten Engine-Entwicklung verwenden und andere Engine-Betreiber autorisieren; die Lizenzgeberin darf sie in ihren genehmigten kommerziellen Engine-, Backend- und Produktangeboten verwenden; berechtigte ADG-Mitglieder dürfen sie innerhalb des Ökosystems nach diesem Vertrag nutzen. Private Zugangsgrenzen gelten weiter. Erforderliche Produkt-Laufzeitbefugnisse dürfen nach Abschnitt 9 an Endnutzer weitergegeben werden. Berechtigte Betreiber müssen die Beitragsverfügbarkeit, das eigenständige Produkteigentum und bestehende Empfängerrechte achten.

7.4 Die Einräumung umfasst Fortführung und Weiterlizenzierung nach Abschnitt 15. Sie können gewährte Beitragsrechte nicht allein deshalb widerrufen, weil Sie ausscheiden, inaktiv werden, den Arbeitgeber wechseln, downgraden, kündigen oder mit der späteren Entwicklung nicht einverstanden sind. Nach Beendigung Ihrer Mitwirkung müssen Sie keine neuen Beiträge schaffen. Zwingende gesetzliche Rechte bleiben unberührt.

7.5 Sie behalten Ihre eigenständig gehaltenen Rechte. Dieser Vertrag behauptet nicht, dass Jan Ihr Werk geschaffen habe, weist ihm nicht alle zukünftigen Werke zu und überträgt das Eigentum an der Engine nicht auf die Lizenzgeberin. Zutreffende Namensnennung und geschützte Urheberrechte bleiben erhalten. Eingeräumt werden die konkreten, für die fortgesetzte gemeinsame Nutzung erforderlichen Befugnisse.

7.6 Soweit Sie sie einräumen dürfen, umfassen die Rechte notwendige Patent- und Gebrauchsmusterbefugnisse zur Nutzung, Herstellung, Herstellung durch Dritte, Änderung, Verbreitung und Bereitstellung des Beitrags und genehmigter Kombinationen. Allein durch ein Ausscheiden entsteht keine neue Patentgebühr oder Rücknahme. Fehlt Ihnen die notwendige Befugnis, legen Sie die Beschränkung vor Einreichung offen und reichen Sie kein unvereinbares Material ein.

7.7 Rechte an enthaltenen Marken, Bildern und anderen Materialien erstrecken sich nur auf ihre erlaubte Nutzung mit dem Beitrag. Sie gestatten keine allgemeine Aneignung des Markenauftritts, der Identität oder einer Unterstützungsbekundung eines Unternehmens. Dieser Vertrag löst bei Ausscheiden keinen Wechsel zu AGPL, Creative Commons oder einer anderen Lizenz aus.

## 8. Herkunft, Drittmaterialien und Verantwortung

8.1 Sie müssen die eingeräumten Rechte besitzen oder über ausreichende Berechtigungen verfügen, einschließlich einschlägiger Rechte aus Arbeitsverhältnissen, Aufträgen, Patenten und von Dritten. Dies gilt auch für KI-unterstützte Arbeit. Legen Sie externe Abhängigkeiten und Bedingungen vor der Integration offen und erhalten Sie erforderliche Hinweise. Reichen Sie Materialien, deren Pflichten der beabsichtigten erlaubten Nutzung widersprechen, nicht ein, bevor dieser Widerspruch gelöst ist.

8.2 Sie sind nach anwendbarem Recht für Ihnen zurechenbare Verstöße verantwortlich. Soweit zulässig, stellen Sie betroffene Begünstigte von begründeten Drittansprüchen und angemessenen notwendigen Verteidigungskosten frei, die durch Ihre schuldhafte Verletzung der Einreichungs- und Berechtigungsanforderungen verursacht werden. Die betroffene Partei muss Sie unverzüglich informieren, angemessene Beteiligung an der Verteidigung ermöglichen und verbindliche Anerkenntnisse oder Vergleiche ohne entsprechende Abstimmung vermeiden. Ihr eigenes Fehlverhalten wird nicht auf Sie abgewälzt.

8.3 Keine Klausel bindet einen außenstehenden Rechteinhaber oder verlagert strafrechtliche Verantwortung. Eine Einreichung, ein Copyright-Dateikopf oder die Prüfung der Lizenzgeberin behebt keine fehlenden Rechte. Strittiges Material kann bis zur Untersuchung isoliert werden, ohne Rechte an nicht betroffenen rechtmäßigen Beiträgen zu beenden.

## 9. Produkte, Veröffentlichung und Kundenvertrieb

9.1 Ihr eigenständiges Produkt verbleibt bei Ihnen. Die Ausnahme von der Quellcodeeinbringung befreit das Produkt nicht von Meldungen und Umsatzbeteiligungen. Ein diesen Bedingungen unterliegendes Produkt ist für diesen Vertrag ein Produkt, das Komponenten enthält oder deren Engine-Funktionalität zur Laufzeit lokal oder über das Backend nutzt. Der bloße Einsatz eines Entwicklungswerkzeugs zur Erstellung eines ansonsten unabhängigen Ergebnisses macht dieses Ergebnis nicht für sich zu einem beteiligungspflichtigen Produkt.

9.2 Sie dürfen vertragsgemäße Produkte vertreiben und Endnutzern die zu deren Nutzung erforderlichen Installations- und Laufzeitbefugnisse erteilen. Private Engine-Laufzeitbestandteile dürfen nur in der ausführbaren oder sonstigen Laufzeitform enthalten sein, die in der Komponentenfreigabe ausdrücklich als verteilbar bezeichnet ist; privater Entwicklungsquellcode und eigenständige Entwicklungsrechte sind ausgeschlossen. Öffentlicher Quellcode bleibt nach Abschnitt 4 weitergebbar. Endnutzer erwerben durch den Kauf Ihres Produkts keinen Enterprise-Zugang.

9.3 Publisher, App-Stores und Distributoren dürfen das Produkt in Ihrem Auftrag im Rahmen Ihrer Rechte vertreiben. Eine Plattformbedingung, die weitergehende Engine-Rechte verlangt, muss vor Ihrer Annahme dieser Bedingung geklärt werden. Ein Vertretungsverhältnis, eine Unterstützungsbekundung oder persönliche Gewährleistung Jans wird nicht unterstellt.

9.4 Informieren Sie die Lizenzgeberin vor der ersten externen Verfügbarkeit eines Produkts über Ihre Identität, das Produkt, Vertriebskanäle, geplantes Datum, betreffende Komponenten und Monetarisierung. Dazu gehören externe Alpha-, Beta-, Test-, Early-Access- und vollständige Launch-Versionen. Interne Entwicklung allein ist keine externe Veröffentlichung. Aktualisieren Sie die Meldung bei wesentlicher Änderung der Produktidentität, Kanäle oder Monetarisierung; gewöhnliche Versionsupdates benötigen keine neu ausgehandelte Lizenz.

9.5 Dokumentieren Sie vor der ersten Monetarisierung die Annahme der kommerziellen Bestimmungen in den Abschnitten 10–11. Bezahlter Alpha-/Beta-Zugang, Early Access und erfasste Vorverkäufe werden nicht bis zu einem offiziellen Launch aufgeschoben. Eine ordnungsgemäße Meldung und Annahme erfordern keine Ermessensentscheidung der Lizenzgeberin über ein Veto gegen eine ansonsten erlaubte Nutzung öffentlicher Komponenten.

9.6 Sie bleiben für Ihr Produkt, seine Nutzer, Inhalte und Kundenzusagen verantwortlich. Halten Sie erforderliche Hinweise an einer geeigneten Stelle für rechtliche Informationen zugänglich. Nichts ermächtigt dazu, Hosting oder Support im Namen Jans persönlich zu versprechen.

## 10. Umsatzbeteiligungsregelung

10.1 Der Beteiligungssatz beträgt **5 %** des beteiligungspflichtigen Umsatzes. Jedes Produkt hat einmalig einen **Umsatzfreibetrag von 100.000 USD über seine gesamte Laufzeit**. Dieser beginnt nicht jährlich, bei einem Plattformwechsel, Versionsupdate, neuen Publisher oder einer Übertragung desselben Produkts auf ein anderes Unternehmen neu. Umsatz oberhalb des Freibetrags ist beteiligungspflichtig; dessen Überschreitung belastet den befreiten Betrag nicht rückwirkend.

10.2 Produktumsatz bezeichnet dem Produkt zurechenbare weltweite Einnahmen: Verkäufe, bezahlter Zugang, Abonnements, Transaktionen innerhalb des Produkts, Werbung, bezahlte Alpha-/Beta-Versionen und Early Access sowie Vorverkäufe oder Crowdfunding-Gegenleistungen für Produktzugang oder Vorteile. Produktbezogene Vorschüsse werden erfasst, soweit sie diesen Einnahmen zurechenbar sind, ohne denselben Betrag bei späterer Verrechnung erneut zu belasten. Unverbundene Unternehmensumsätze und echte Spenden ohne Produktgegenleistung sind ausgeschlossen.

10.3 Berechnen Sie Einnahmen vor Plattformprovisionen, Publisheranteilen und Entwicklungs- oder Betriebskosten. Ziehen Sie gesondert ausgewiesene, tatsächlich an Behörden zu zahlende Transaktionssteuern und tatsächliche Kundenerstattungen ab. Rückabwicklungen müssen nachgewiesen und dem Produkt zugeordnet werden, ohne doppelte Abzüge. Eine spätere Erstattung zuvor beteiligungspflichtiger Einnahmen wird dem Beteiligungskonto dieses Produkts gutgeschrieben; ein bei der Schlussabrechnung verbleibendes Guthaben ist erstattungsfähig und verfällt nicht stillschweigend.

10.4 Sämtliche Plattformfassungen und Updates desselben Produkts werden zusammengefasst. Tatsächlich eigenständige Produkte haben eigenständige Freibeträge. Pakete und Erweiterungen sind nach einer angemessenen, dokumentierten und einheitlich angewandten Grundlage entsprechend ihrem wirtschaftlichen Zusammenhang zuzuordnen. Teilen oder benennen Sie ein Produkt nicht um, um Freibeträge zu vervielfachen. Wesentliche Zuordnungsunsicherheiten sind mit der Meldung offenzulegen.

10.5 Ein Kalenderquartal mit Produktumsatz **unter 1.000 USD** löst keine Beteiligung aus. Dieser Umsatz zählt dennoch zum laufzeitbezogenen Freibetrag. Bei genau 1.000 USD oder darüber gilt diese Quartalsbefreiung nicht. Es handelt sich um eine Freigrenze, nicht um einen zusätzlichen Abzug von 1.000 USD.

10.6 Andernfalls entspricht der beteiligungspflichtige Umsatz dem Produktumsatz des Quartals abzüglich des zu Quartalsbeginn noch nicht verbrauchten Laufzeitfreibetrags, jedoch mindestens null. Der ungenutzte Freibetrag beträgt 100.000 USD abzüglich des zuvor kumulierten Produktumsatzes, jedoch mindestens null. Erstattungskorrekturen sind so abzustimmen, dass weder Einnahmen noch Freibetragsverbrauch doppelt gezählt werden.

10.7 Beispiele: Bei 80.000 USD bisherigem kumuliertem Umsatz und 30.000 USD im Quartal beträgt die Beteiligung 500 USD. Ist der Freibetrag ausgeschöpft, löst ein Quartal mit 800 USD keine Beteiligung aus; ein Quartal mit 2.000 USD löst 100 USD aus. Diese Beispiele setzen voraus, dass keine späteren Korrekturen erfolgen.

10.8 Backend-Entgelte sind von Umsatzbeteiligungen unabhängig und werden nicht automatisch darauf angerechnet. Diese Regelung begründet keine Gebühr pro Installation, keinen neuen Beteiligungssatz, keinen besonderen Store-Rabatt und kein zusätzliches Pflichtabonnement für öffentlichen Code.

## 11. Meldung, Währung und Zahlung

11.1 Melden Sie ab der ersten Monetarisierung quartalsweise Produktumsatz, Abzüge, kumulierten Umsatz, Freibetragsverbrauch und Beteiligung innerhalb von **45 Kalendertagen nach Quartalsende**. Zahlen Sie eine geschuldete Beteiligung innerhalb derselben Frist. Eine Meldung ohne Zahlung muss erkennen lassen, warum keine Zahlung geschuldet ist. Ein inaktives Produkt kann mit seiner abschließenden Meldung als inaktiv gekennzeichnet werden; melden Sie eine erneute Monetarisierung bei Wiederaufnahme.

11.2 Bewahren Sie einschlägige Abrechnungen und Belege während der geltenden gesetzlichen Aufbewahrungsfristen auf. Die Lizenzgeberin darf angemessene Nachweise zu einer festgestellten Abweichung anfordern. Jede Prüfung muss vertrauliche Geschäfts- und Personendaten schützen, auf einschlägige Umsätze beschränkt bleiben und darf Ihren ausgenommenen Produktquellcode nicht verlangen. Weitergehende Prüfungen bedürfen eines gesondert vereinbarten Verfahrens.

11.3 USD ist die Referenzeinheit für Schwellen und Berechnungen. Verwenden Sie für BRL-Einnahmen den USD/BRL-PTAX-Verkaufsschlusskurs der Banco Central do Brasil am Einnahmetag oder den zuletzt davor veröffentlichten Geschäftstagskurs. Für andere Währungen verwenden Sie, soweit verfügbar, einen belegten tatsächlichen USD-Abrechnungskurs; andernfalls ist die entsprechende veröffentlichte Zentralbank-Referenzumrechnung einheitlich anzuwenden und Quelle und Methode offenzulegen. Wählen Sie Kurse nicht opportunistisch zwischen Transaktionen aus.

11.4 Ist eine Zahlung in BRL erforderlich, rechnen Sie den USD-Betrag mit dem BCB-Verkaufsschlusskurs des Geschäftstags unmittelbar vor Rechnungsstellung um. Die Lizenzgeberin stellt rechtmäßige Rechnungs- und Zahlungsinformationen rechtzeitig zur Zahlung bereit. Zwingende Währungs-, Steuer- und Einbehaltungsvorschriften gehen vor. Gesetzlich vorgeschriebene, amtlich belegte Einbehalte werden berücksichtigt; dieser Vertrag begründet keine automatische Erhöhung des Zahlbetrags zum Ausgleich von Steuern.

11.5 Übermitteln Sie Meldungen über den bezeichneten privaten Kundenkanal oder den bei Aufnahme mitgeteilten vertraulichen Kontakt. Fehlt ein elektronischer Kanal oder fällt er aus, kann eine Mitteilung an den in Abschnitt 1 bezeichneten Sitz zugestellt werden. Legen Sie private Umsätze niemals in einem öffentlichen GitHub-Issue offen. Ein Zugangsausfall darf nicht zur künstlichen Herbeiführung eines Verzugs verwendet werden, wenn eine rechtzeitige Meldung über eine verfügbare Alternative in angemessener Weise versucht wurde.

11.6 Berichtigen Sie erkannte Fehler ohne schuldhaftes Zögern. Eine verspätete Meldung beseitigt den Laufzeitfreibetrag nicht automatisch. Tatsächliche Verstöße unterliegen weiterhin Abschnitt 14 und dem anwendbaren Recht. Ein neuer Website-Preis begründet keine rückwirkende Beteiligung.

## 12. PaaS- und Enterprise-Leistungen

12.1 Mit PaaS werden Backend-Bereitstellung und die vereinbarten Portal-, Datenbank-, API- und Hostingressourcen erworben. Standardkunden nutzen die autorisierten Portale und Schnittstellen; Enterprise-Kunden erhalten zusätzlich ihre bezeichneten privaten Quellcoderechte. Die verbindliche Bestellung nennt gekaufte Ressourcen, Preise, Abrechnungsperiode, Support und Leistungsumfang. Dieser Vertrag ist nicht selbst eine Hostingbestellung.

12.2 Angaben zu Verfügbarkeit, technischer Gültigkeit, Datenbehandlung, Export, Sicherung und Aufbewahrung müssen nach Bedarf in Kundenbedingungen und Bestellung bereitgestellt werden. Allein aus einer Tarifbezeichnung wird keine private Lizenz oder Supportzusage abgeleitet. Zwingende Pflichten zur Softwarevermarktung bleiben unberührt.

12.3 Eine Bestellung darf die von ihr eingeräumten privaten Entwicklungsrechte begrenzen. Sie darf die öffentlichen Befugnisse aus Abschnitt 4 nicht allein wegen des Endes eines Abonnements aufheben. Ein Kunde bleibt nach Kündigung berechtigt, ein ansonsten vertragsgemäßes Produkt ausschließlich mit unabhängig lizenzierten öffentlichen Komponenten zu entwickeln.

## 13. Kündigung, Downgrade und bestehende Veröffentlichungen

13.1 Sie können jederzeit ein Downgrade beantragen, wirksam zum Ende der laufenden vereinbarten Abrechnungsperiode. Eine Monatsperiode endet zu ihrem vereinbarten Monatsende, eine Jahresperiode zu ihrem vereinbarten Jahresende. Bestellung und zwingendes Recht bestimmen Kündigungsmitteilung und etwaige Erstattung; die Möglichkeit eines Downgrade-Antrags verkürzt den erworbenen Zeitraum nicht.

13.2 Beim Downgrade gelten die Rechte und Ressourcen des neuen Tarifs. Nicht mehr enthaltene private Rechte enden für die Zukunft. Bei Kündigung oder Aussetzung werden betroffenes Backend und Zugang nach dem Vertrag eingestellt. Der Besitz eines privaten Checkouts begründet weder fortdauernde Entwicklungsrechte noch eine Selbsthosting-Lizenz.

13.3 Sie dürfen die unveränderte, rechtmäßig veröffentlichte Produktversion innerhalb fortbestehender Vertriebsrechte weitervertreiben. Endnutzer dürfen die erlaubte Laufzeitnutzung fortsetzen. Umsatzbeteiligungen und Meldungen gelten weiter. Dies verspricht keine fortgesetzten  
Backend-Leistungen: Ein Offline-Produkt kann unabhängig funktionieren, während ein backendabhängiges Produkt seine Funktionsfähigkeit verlieren kann.

13.4 Sie dürfen mit abgelaufenen privaten Entwicklungsrechten kein neues Produkt und keine neue Version erstellen, einschließlich eines als Wartungsupdate bezeichneten Updates, das diese Rechte benötigt. Sie dürfen die Arbeit mit einer geeigneten Berechtigung wieder aufnehmen oder innerhalb weiterhin bestehender Rechte entwickeln. Arbeiten ausschließlich mit rechtmäßig verfügbaren öffentlichen Komponenten bleiben nach Abschnitt 4 ohne Abonnement erlaubt.

13.5 Bewahren Sie private Kopien nur auf, soweit dies für fortbestehende Vertriebsrechte, erforderliche Nachweise oder autorisierte Archive erlaubt ist. Aufbewahrung erlaubt keine neue Entwicklung. Erfüllen Sie die einschlägigen Rückgabe- und Löschpflichten, ohne gesetzlich vorgeschriebene Unterlagen zu vernichten. Das Ende eines Abonnements oder einer Mitgliedschaft entzieht keine bereits eingeräumten Beitragsrechte.

## 14. Verstöße, Gewährleistung und Verantwortung

14.1 Die Lizenzgeberin darf die Beendigung und Behebung eines festgestellten Verstoßes verlangen. Bei einem behebbaren Verstoß wird grundsätzlich eine mit der Mitteilung bezeichnete angemessene Abhilfemöglichkeit gewährt, entsprechend seiner Schwere und dem erforderlichen Aufwand. Eine sofortige gezielte Zugangssperre kann eingesetzt werden, soweit dies bei schwerwiegender unbefugter Offenlegung, Rechtsverletzung oder Sicherheitsrisiken erforderlich ist. Eine Kündigung aus wichtigem Grund richtet sich nach anwendbarem Recht; ein geringfügiger technischer Fehler beendet nicht automatisch jede Lizenz.

14.2 Beitragsrechte, Vertraulichkeit, entstandene Melde- und Zahlungspflichten sowie wirksame bestehende Drittrechte bestehen im Umfang ihrer Bedingungen fort. Abschnitt 13 garantiert nicht, dass ein rechtswidrig vertriebenes Produkt nach Kündigung wegen schwerwiegender Rechtsverletzung weitergeführt werden darf.

14.3 Kostenlose Entwicklungsmaterialien werden in ihrem vorhandenen Zustand ohne vertragliche Zusage von Eignung, Fertigstellung oder fehlerfreiem Betrieb bereitgestellt, sofern eine solche nicht ausdrücklich erteilt wird. Zusagen zu entgeltlichen Leistungen und zwingende Rechte bleiben unberührt. Sie müssen die Eignung für Ihr beabsichtigtes Produkt beurteilen.

14.4 Soweit gesetzlich zulässig, ist die Haftung für mittelbare Schäden, Folgeschäden und entgangenen Gewinn ausgeschlossen. Dies schließt Betrug, vorsätzliches oder grob fahrlässiges Verhalten, Personenschäden, eine verbindliche Garantie, zwingende Haftung oder für wesentliche Vertragspflichten rechtlich nicht ausschließbare Haftung nicht aus. Verbraucherschutz wird nicht verdrängt. Eine willkürliche finanzielle Haftungsobergrenze wird hier nicht festgesetzt.

14.5 Jan garantiert nicht persönlich die Schulden oder Kundenverträge der Gesellschaft; bei Ausfall der Gesellschaft geht keine Leistungspflicht automatisch auf ihn über. Jede Partei bleibt für ihr eigenes rechtlich zurechenbares Verhalten verantwortlich. Dieser Vertrag kann nicht sämtliche Drittansprüche verhindern oder zwingende gesetzliche Haftung verdrängen.

## 15. Fassungen und Projektfortbestand

15.1 Jan Ohlmann ist der bezeichnete Herausgeber künftiger LIC-Fassungen. Jede muss eine Kennung, ein Datum und eine Änderungsdokumentation haben. Beitragsbefugnisse umfassen die Befugnis zur Fortführung und Unterlizenzierung tatsächlich eingereichter Beiträge unter späteren von ihm herausgegebenen Fassungen, die gemeinsame Engine-Entwicklung, Verfügbarkeit für berechtigte Community-Empfänger, die Ausnahme eigenständiger Produkte und bereits gewährte Beitragsbegünstigungen bewahren. Für Nutzungen innerhalb dieser wirksam eingeräumten Befugnis ist keine weitere Unterschrift des Beitragenden erforderlich.

15.2 Diese Befugnis begründet keine beliebigen neuen Zahlungs-, Haftungs- oder Leistungspflichten bestehender Nutzer. Änderungen ihrer Verträge bedürfen einer rechtmäßigen Grundlage. Schweigen oder der Commit einer anderen Person ist keine pauschale Zustimmung. Bereits gewährte Befugnisse und eigenständige Drittlizenzen können nicht rückwirkend durch Austausch einer Datei entzogen werden.

15.3 Die ADG und wirksame Beitragsrechte werden nicht allein deshalb aufgelöst, weil ein operatives Unternehmen seinen Betrieb einstellt. Kontrolländerungen der Gesellschaft übertragen weder Jans privates Eigentum noch ein automatisches Recht auf zukünftige Versionen. Die gesonderte vorgelagerte Lizenz und zwingende Insolvenzregeln bestimmen die Stellung der Gesellschaft. Ein anderer Betreiber benötigt eine eigene Berechtigung; dieser Vertrag gewährt keinen allgemeinen Franchise-Anspruch.

## 16. PJV, Mitteilungen und anwendbares Recht

16.1 Beitragende müssen auch die bezeichnete PJV annehmen. Die LIC regelt Beitragsrechte, Beteiligungsbedingungen, öffentliche und private Befugnisse sowie Produktgrenzen; die PJV regelt Teilnahme und Projektablauf. Keines der Dokumente ändert stillschweigend den Gesellschaftsvertrag. Für neu unter diesen Fassungen angenommene Beiträge gelten widersprechende Widerrufs- und automatische Lizenzwechselregeln der LIC/PJV von 2016 nicht. Allein die Veröffentlichung schreibt einen alten Vertrag dadurch nicht rückwirkend um.

16.2 Individuelle Bedingungen dürfen Angelegenheiten innerhalb der tatsächlichen Befugnisse der Parteien abweichend regeln, jedoch nicht über Rechte anderer Rechteinhaber verfügen. Die Verarbeitung personenbezogener Daten benötigt eine eigene rechtmäßige Grundlage und erforderliche Datenschutzinformationen. Dies ersetzt keine Vereinbarung zur Datenverarbeitung, wenn eine solche erforderlich ist.

16.3 Mitteilungen müssen in angemessener Weise schriftlich oder in elektronischer Textform über den jeweiligen privaten Vertragskanal oder den Gesellschaftssitz nachweisbar sein. Gewöhnliche Fragen zu öffentlichem Code können Repository-Kanäle nutzen. Eine technische Projektfreigabe ist keine persönliche Garantie und kein Verzicht auf finanzielle Bedingungen.

16.4 Brasilianisches Recht gilt, soweit eine Rechtswahl wirksam ist. Zwingende Schutzregeln für eine Person oder Transaktion und nicht abdingbare gerichtliche Zuständigkeiten bleiben anwendbar. Diese Rechtswahl entzieht einem Verbraucher nicht den Schutz der zwingenden Vorschriften des Rechts des Staates, in dem er seinen gewöhnlichen Aufenthalt hat. Soweit eine Gerichtsstandsvereinbarung rechtmäßig ist, werden die Gerichte von Caucaia, Ceará, Brasilien bestimmt. Es wird weder eine Schiedsgerichtspflicht noch ein Verzicht auf zwingende kollektive Rechtsbehelfe auferlegt.

16.5 Dieser Vertrag liegt in brasilianischem Portugiesisch (pt-BR), Englisch und Deutsch mit gleichem Inhalt vor. Maßgeblich ist die pt-BR-Fassung; weichen die Sprachfassungen voneinander ab, geht die pt-BR-Fassung vor. Die englische und die deutsche Fassung werden vor Vertragsschluss stets zusammen mit der pt-BR-Fassung bereitgestellt und mit ihr auf der Webseite der Lizenzgeberin veröffentlicht. Soweit anwendbares Recht portugiesische oder andere verständliche vorvertragliche Informationen verlangt, muss die Lizenzgeberin sie vor Vertragsschluss bereitstellen. Kein Sprachvorrang verdrängt zwingende Auslegungs- oder Informationspflichten, einschließlich der Verbraucherschutzvorschriften des Staates des gewöhnlichen Aufenthalts. Eine Übersetzung muss ihre zugehörige Fassung und einen ausdrücklich vereinbarten maßgeblichen Status bezeichnen.

16.6 Ist eine Klausel unwirksam, bestehen die übrigen Bestimmungen nur im gesetzlich zulässigen Umfang fort. Ein unwirksamer Ausschluss wird nicht automatisch auf die maximal mögliche beschränkende Wirkung umgeschrieben. Es gelten die einschlägigen gesetzlichen Ersatzregelungen.

16.7 Diese Fassung wird erst durch autorisierte Veröffentlichung, nachdem die Lizenzgeberin die erforderlichen Rechte besitzt, zu einem Angebot für eine Komponente. Ihr Ausgabedatum behauptet nicht, dass ein vorgelagerter Vertrag bereits unterzeichnet wurde. Unveränderte Kopien dürfen lizenziertes Material begleiten. Nicht genehmigte Änderungen dürfen nicht als offizielle Antares-Lizenz dargestellt werden.

Urheberrecht am Lizenztext: © 2026 Jan Ohlmann. Das Urheberrecht an lizenzierten Materialien verbleibt bei den jeweiligen Rechteinhabern.
