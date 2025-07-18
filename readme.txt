--- Makefile ---
make all: Αρχικά φτιάχνει το φάκελο bin και τοποθετεί μέσα σε αυτόν όλα τα εκτελέσιμα αρχεία. Έπειτα παράγει τα εκτελέσιμα αρχεία fss_manager, fss_console, worker.
make clean: Διαγράφει το bin φάκελο και ότι εκτελέσιμα αρχεία περιέχονται σε αυτόν.
make <fss_manager/fss_console/worker>: Παράγει το εκτελέσιμο αρχείο της επιλογής μας (δημιουργεί το φάκελο bin αν δεν υπάρχει).
make clean-<fss_manager/fss_console/worker>: Διαγράφει το εκτελέσιμο αρχείο της επιλογής μας.

ΠΑΡΑΤΗΡΗΣΗ: Οι fss_manager και fss_console πρέπει να εκτελεστούν από το root κατάλογο του repository. Δηλαδή, './bin/fss_manager ...' και './bin/fss_console ...'

--- worker.cpp ---
Διαχειρίζεται τις εργασίες συγχρονισμού που του αναθέτονται είτε από την inotify(μέσω του fss_manahger), είτε από τη χρήστη μέσω command στο fss_console.

operation_stats struct: Αποθηκεύονται σε αυτή πόσα αρχεία αντιγράφθηκα, προσπεράστηκαν(λόγω σφάλματος) ή διαγράφτηκαν(δες deleteObsoleteFile) + αν η εργασία του συγχρονισμού ήταν επιτυχής ή όχι.

copyFile(): Συνάρτηση που της δίνουμε σαν όρισμα το path ενός αρχείου, στο φάκελο που βρίσκεται και στο φάκελο που θέλουμε να το αντιγράψουμε. Αντιγράφει το αρχείο σε κομμάτι (αν το αρχείο προορισμού δεν υπάρχει, το δημιουργεί, ενώ αν υπάρχει ήδη, το αντικαθιστά με το νέο περιεχόμενο).
deleteObsoleteFile(): Διαγράφει τα αρχεία που βρίσκονται στο target directory και όχι στο source.
printReport(): Eκτυπώνει την αναφορά της εργασία συχρονισμού στο stdout (βασισμένο στο παράδειγμα που μας δώθηκε).
operationFullSync(): Αναλαμβάνει το πλήρη συγχρονισμό μεταξύ δύο φακέλων είτε επειδή ξεκίνησε η παρακολούθηση τους, έιτε επειδή το ζήτησε ο χρήστης. Δημιουργεί τα paths του κάθε αρχείου στους δύο φακέλους και τα δίνει στην copyFile() για να αναλάβει την αντιγραφή τους. Έπειτα, διαγράφει τα αρχεία που βρίσκονται στο target directory και όχι στο source με την deleteObsoleteFile().
operationWrite(): Αναλαμβάνει το operation ADDED ή MODIFIED, με τη βοήθεια της copyFile().
operationDelete(): Αν διαγραφεί ένα αρχείο από το source directory, αναλαμβάνει να το διαγράφει(unlink) και από το target.
main(): Με βάση τα ορίσματα και το operation που της δωθεί καλεί την αντίστοιχη συνάρτηση, καλεί την printReport και επιστρέφει αν ήταν επιτυχής ή όχι η δουλειά που έκανε.

--- fss_console ---
Εντολή εκτέλεσης από το root directory: ./bin/fss_console -l <log_file>

Αναλαμβάνει την αποστολή των εντολών του χρήστη στο fss_manager μέσω named pipes, καθώς και την εμφάνιση απαντήσεων για κάθε μια από αυτές. Όλα αυτά τα καταγράφει ταυτόχρονα, σε log αρχείο που δίνει ο χρήστης σαν όρισμα.

--- fss_manager ---
Εντολή εκτέλεσης από το root directory: ./bin/fss_manager -l <log_file> -c <config_file> -n <worker_limit>

Ο fss_manager αποτελεί τον κεντρικό διαχειριστή του συστήματος συγχρονισμού. Δημιουργεί τα named pipes για την επικοινωνία με το fss_console, από όπου λαμβάνει εντολές χρήστη. Διαβάζει το config file και εκκινεί workers για τον αρχικό συγχρονισμό των φακέλων, ενώ παράλληλα προσθέτει inotify watches για τη συνεχή παρακολούθησή τους. Μέσω ενός infinite loop με χρήση polling, διαχειρίζεται ταυτόχρονα εισερχόμενες εντολές από το fss_console, events από το inotify και syscalls, ώστε να ξεκινά, να τερματίζει ή να προγραμματίζει εργασίες συγχρονισμού, διασφαλίζοντας την ομαλή και αποδοτική λειτουργία του συστήματος.

--- message_utils.cpp ---
Περιέχει ότι συναρτήσεις έχουν να κάνουν με τη δημιουργία timestamps, την ενσωμάτωση τους σε ένα μήνυμα αλλά και συναρτήσεις για τη φόρτωση μηνυμάτων σε buffer και την αποστολή τους σε log file ή pipe.

getTimestamp(): Δεσμέυει δυναμικά χώρο και αποθηκέυει σε αυτόν την τρέχουσα ημερομηνία και ώρα. Επιστρέφει char*.
addTimestampToMessage(): Ενσωματώνει στην αρχή του μηνύματος το timestamp που είτε του δώσαμε εμείς, είτε παράχθηκε από την getTimestamp().
appendToBuffer(): Προσθέτει στο τέλος του buffer, το μήνυμα που του δώσουμε.

ΠΑΡΑΤΗΡΗΣΗ: Οι συναρτήσεις addTimestampToMessage() και appendToBuffer() χρησιμοποιούν τη realloc, επομένως ο pointer που επιστρέφουν μπορεί να διαφέρει από αυτόν που τους δόθηκε ως όρισμα. Για αυτό, θα πρέπει πάντα να χρησιμοποιείται ο επιστρεφόμενος δείκτης ως ο "ενημερωμένος" pointer του μηνύματος ή buffer.

forwardMessage(): Αντιγράφει στο log file και στο pipe το μήνυμα που του δώσουμε. Αν το μήνυμα έχει μεγαλύτερο μέγεθος από αυτό που χωράει στο pipe, το στέλνουμε τμηματικά.

--- sync_database.cpp ---
Συναρτήσεις για την αποθήκευση δεδομένων σχετικά με τους καταλόγους που παρακολουθούμε.

In sync_database.h -> sync_info_entry struct: Δομή που περιέχει πληροφορίες κάθε καταλόγου παρακολούθησης. Εδώ αποθηκεύονται, το path του φακέλου παρακολούθησης και του καταλόγου που αποθηκεύονται τα συγχρονισμένα δεδομένα, η τελευταία φορά που έγινε συγχρονισμός, ο αριθμός των σφαλμάτων που έχουν εντοπιστεί απο εργασίες συγχρονισμού του και τέλος ο watcher που παράγει το inotify για να παρακολουθεί τις αλλαγές που γίνονται στο φάκελο. Οι πληροφοριές αποθηκεύονται σε ένα global unordered με όνομα map sync_info και σαν κλειδί χρησιμοποιείται το path του φάκελου που παρακολουθούμε(μιας και είναι unique).

readConfig(): Διαβάζει τους καταλόγους που βρίσκονται μέσα στο config file και με τη χρήση της addSyncInfo() προσθέτει το ζευγάρι και αρχικοποιεί τα δεδομένα του μέσα στο unordered map.
addSyncInfo(): Δεσμεύει δυναμικά χώρο για την αποθήκευση δεδομένων του και προσθέτει το κατάλογο στo unordered map.
getSyncInfo(): Επιστρέφει τις πληροφορίες του καταλόγου παρακολούθησης που του δώσουμε (αν υπάρχουν), μέσα από το unordered map.
rmvSyncInfo(): Διαγράφει τις πληροφορίες του καταλόγου παρακολούθησης που του δώσουμε, από το unordered map. (χρησιμοποιείται από το delete command, δεν είναι μέσα στις απαιτήσεις της εργασίας)
copySyncInfo(): Δεσμεύει χώρο, γράφει μέσα σε αυτό με τις πληροφορίες του καταλόγου που του δώσαμε (formatted message) και το επιστρέφει.
printAllSyncInfo(): Εκτυπώνει όλες τις πληροφορίες που είναι αποθηκευμένες στο unordered map.
cleanupAllSyncInfo(): Αποδεσμεύει τη μνήμη που δεσμεύεται από το unordered map και από τις πληροφορίες που είναι αποθηκευμένες μέσα σε αυτό.

--- monitor_manager.cpp ---
Συναρτήσεις σχετικά με την inotify και την παρακολούθηση των καταλόγων 

initMonitorManager(): Αρχικοποιεί την inotify.
addDirToMonitor(): Αρχίζει τη παρακολούθησει ενός καταλόγου(inotify_add_watch) και επιστρέφει το watcher του.
rmvDirFromMonitor(): Σταματάει τη παρακολούθησει ενός καταλόγου(rmvDirFromMonitor).
handleDirChange(): Όταν η inotify εντοπίζει αλλαγές στο κατάλογο παρακολούθησης, η συνάρτηση αναλαμβάνει ένα ένα τα events και ανάλογα με την αλλαγή που εντοπίστηκε βάζει στην ουρά(see task_manager.cpp) μια εργασία συγχρονισμού μέσω της addTaskToQueue(). 
shutdownMonitorManager(): Σταματάει τη παρακολούθηση όλων των καταλόγων που βρίσκονται μέσα στο unordered map.

--- task_manager.cpp ---
Συναρτήσεις που έχουν να κάνουν με τη διαχείριση έναρξης/τερματισμού των workers καθώς και της ουράς αναμονής που βάζουμε τις εργασίες συγχρονισμού όταν όλοι οι workers είναι δεσμευμένοι. Συγκεκριμένα, όλες οι εργασίες μπαίνουν πρώτα στην ουρά αναμονής (ακόμα και αν όλοι οι workers είναι ελεύθεροι) και στη συνέχεια τις στέλνουμε στους workers.

In task_manager.h -> task_t struct: Δομή της εργασίας συγχρονισμού, που αποθηκεύουμε τα ορίσματα που θα δώσουμε στο worker όταν έρθει η σειρά της να εκτελεστεί.
In task_manager.h -> worker_info_t struct: Δομή για τους ενεργούς worker που αποθηκεύουμε το pid τους, το pipe από το οποίο θα λάβουμε το output τους και το task_t struct σχετικά με την εργασία που επεξεργάζονται εκείνη τη στιγμή. (είναι ένας δυναμικά δεσμευμένος πίνακας)

task_queue: H ουρά που αποθηκεύουμε τα task_t πριν τα δώσουμε σε κάποιο worker.

vvv Κύριες Συναρτήσεις vvv
initWorkerManager(): Ορίζει το μέγιστο αριθμό workers, και δεσμεύει χώρο για το πίνακα που θα αποθηκεύουμε πληροφορίες σχετικά με αυτούς. Επιπλεόν, αρχικοποιεί το singal handler για όταν τελείωνει ένας worker με τη βοήθειας της signalHandler().
addTaskToQueue(): Προσθέτει την εργασία συγχρονισμού στην ουρά αναμονής. Αν η εργασία συγχρονισμού έρχεται από το console, τσεκάρουμε πρώτα αν υπάρχει ήδη εργασία για τον ίδιο κατάλογο και αν υπάρχει δεν τη βάζουμε στην ουρά.
isTaskQueued(): Επιστρέφει true/false, για το αν υπάρχει στην ουρά αναμονής ή εκτελείται εκείνη τη στιγμή από κάποιο worker, εργασία συγχρονισμού για το κατάλογο που δώσαμε ως όρισμα.
startWorker(): Ελέγχει αν είναι ελεύθερος κάποιος worker και βάζει με τη σειρά τις εργασίες που περιμένουν στην ουρά. Για την έναρξη μιας εργασία, αρχικοποιούμε το pipe, καλούμε τη fork και μετά την execv (δίνοντας της μέσα σε ένα πίνακα τα απαραίτητα ορίσματα) για να ξεκινήσει o worker. Από πλευράς του ο manager, αποθηκεύει τις πληροφορίες του worker στο πίνακα worker_info_t.
processFinishedWorker(): Παίρνει έναν έναν τους workers που έχουν τελειώσει, λαμβάνει το output τους μέσω pipe και καλεί τη processWorkerOutput() για να το επεξεργαστεί. Στη συνέχεια ενημερώνει το χρόνο που συγχρονίστηκε τελευταία φορά ο συγκεκριμένος κατάλογος, ενημερώνει τη worker_info_t με τη βοήθεια της copyTask() και αποδεσμεύει το χώρο που είχαν αποθηκευτεί οι πληροφορίες της εργασίας συγχρονισμού.
finishTasks(): Συνάρτηση που χρησιμοποιείται κατά τον τερματισμό του manager. Εκτελεί και ολοκληρώνει με ομαλό/σωστό τρόπο όλες τις εργασίες που περιμένουν στην ουρά και στους workers.
shutdownWorkerManager(): Αποδεσμεύει το χώρο που είχαμε δεσμεύεσει για τις εργασίες που βρίσκονται μέσα στους workers και στην ουρά αναμονής.

vvv Βοηθητικές Συναρτήσεις vvv
signalHandler(): Αλλάζει τη τιμή του worker_finished_flag σε 1, όταν τελειώνει ένας worker. (στη συνέχεια πρέπει να καλέσουμε τη processFinishedWorker())
setupSignalHandler(): Αρχικοποίηση του signalHandler().
processWorkerOutput(): Ανακατευθύνει το stdout του worker στο pipe, επεξεργάζεται το report του και καταγράφη την εργασία του worker και πληροφορίες σχετικά με αυτόν στο log file του manager. (αν η εργασία ήταν sync που είχε σταλθεί απο το console εκτύπωσε και στα terminals σχετικό μήνυμα)
initTask(): Δεσμεύει χώρο και αποθηκεύει τις πληροφορίες μιας εργασίας σε μια task_t struct.
freeTaskMemory(): Αποδεσμεύει το χώρο που είχε δεσμευτεί για να αποθηκευτούν οι πληροφορίες μιας εργασίας.
copyTask(): Χρησιμοποιείται μέσα στη processFinishedWorker() για να μετακινήσει τις πληροφορίες των ενεργών workers μια θέση πίσω στο πίνακα που είναι αποθηκευμένες. Δηλαδή αν τερματίσει ένας worker που βρίσκεται στη μέση του πίνακα worker_info_t, το "κενό" καλύπτεται μετακινόντας τους υπόλοιπους ενεργούς workers.

--- commands.cpp ---
Συναρτήσεις που διαχειρίζονται τα commands που στέλνει ο χρήστης μέσω του console. 

commandAdd(): Αν ένας κατάλογος δεν είναι ήδη αποθηκευμένος στο sync_info τον αποθηκεύει, δημιουργεί έναν inotify watcher για να παρακολουθούμε τις αλλαγές του και βάζει στην ουρά μια εργασία πλήρη συγχρονισμού του. Αν είναι ήδη αποθηκεύμένος αλλά δεν παρακολουθείται, δημιουργεί έναν inotify watcher για να παρακολουθούμε τις αλλαγές του και βάζει στην ουρά μια εργασία πλήρη συγχρονισμού του.
commandCancel(): Σταματάει τη παρακολούθηση ενός καταλόγου(σβήνει τον watcher). Σε περίπτωση που υπάρχουν στην ουρά εργασίες συγχρονισμού, δεν σταματάμε τη παρακολούθηση του.
commandStatus(): Με τη βοήθεια της copySyncInfo(), εκτυπώνουμε στα τερματικά της πληροφορίες του καταλόγου που δώσουμε σαν όρισμα. Αν για όρισμα δωθεί 'all', εκτυπώνονται οι πληροφορίες όλων των καταλόγων που είναι αποθηκευμενές στο sync_info (όχι απαίτηση της εργασίας).
commandSync(): Στέλνει στην ουρά αναμονής με τη βοήθεια της addTaskToQueue() μια εργασία πλήρης συγχρονισμού για τον κατάλογο που έχουμε δώσει ως όρισμα. Αν υπάρχει ήδη άλλη εργασία συγχρονισμού για τον ίδιο κατάλογο, δεν βάζουμε την καινούργια στην ουρά. Σε περίπτωση που ζήτηθεί sync για έναν ανενεργό κατάλογο, καλούμε την commandAdd() χωρίς να δώσουμε target κατάλογο. ΣΗΜΕΙΩΣΗ, το operation που περνάμε στο worker για να γίνει ο πλήρης συγχρονισμό πρέπει να είναι 'SYNC' και όχι 'FULL' όπως θα κάναμε κανονικά, έτσι ώστε η συνάρτηση που διαχειρίζεται το τερματισμό του worker να εκτυπώσει το μήνυμα τερματισμού 'Sync completed...', πέρα από αυτό και τα δύο operation κάνουν ακριβώς το ίδιο.
commandDelete(): ΔΕΝ ΕΙΝΑΙ ΜΕΡΟΣ ΤΩΝ ΑΠΑΙΤΗΣΕΩΝ ΤΗΣ ΕΡΓΑΣΙΑΣ! Διαγράφει από την sync_info τον κατάλογο που θα δώσουμε ως όρισμα. (ο κατάλογος δεν πρέπει να παρακολουθείτε εκείνη τη στιγμή)
commandShutdown(): Καλείται όταν ξεκινήσει ο τερματισμός του manager. Καλεί τη finishTasks() για να τελειώσουν όλες οι εργασίες συγχρονισμού πριν τερματιστεί το πρόγραμμα.

--- fss_script.sh ---
Εντολή εκτέλεσης: ./fss_script.sh -p <path> -c <command>

Με βάση τα ορίσματα που δώσουμε εκτελούνται οι εντολές purge/listAll/listMonitored/listStopped

purge: Διαγράφει το αρχείο ή κατάλογο που δώσουμε στο όρισμα path.
listAll: Τα log αρχεία έχουν συγκεκριμένη δομή για αυτό το λόγω με τη χρήση awk μπορούμε εύκολα να βρούμε ποιές σειρές μηνυμάτων μας αφορούν. Για να βρούμε τη πιο πρόσφατη εργασία συγχρονισμού ενός καταλόγου το μόνο που έχουμε να κάνουμε είναι να γράφουμε πάνω στις ίδιες μεταβλητές τα πιο πρόσφατα δεδομένα που βρίσκουμε για το κάθε κατάλογο. Οι πληροφορίες που μας μένουν όταν φτάσουμε στο τέλος του αρχείου, είναι και αυτές που πρέπει να εκτυπώσουμε.
listMonitored/listStopped: Λειτοργούν με παρόμοιο τρόπο όπως και η listAll
