#include <iostream>
#include <queue>
#include <deque>
#include <vector>
#include <string>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <ctime>
#include <chrono>
#include <algorithm>
#include <regex>
#include <thread>

// ---------- cross-platform sockets ----------
// The server runs on Windows (Winsock) and on POSIX systems (Berkeley
// sockets). Keeping the abstraction small lets the same file build in
// both environments, which is what CI on Linux relies on.
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    static const socket_t INVALID_SOCK = INVALID_SOCKET;
    inline int closeSocket(socket_t s) { return closesocket(s); }
    inline void initSockets() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    inline void cleanupSockets() { WSACleanup(); }
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using socket_t = int;
    static const socket_t INVALID_SOCK = -1;
    inline int closeSocket(socket_t s) { return close(s); }
    inline void initSockets() { /* nothing to initialise on POSIX */ }
    inline void cleanupSockets() { /* nothing to clean up on POSIX */ }
#endif

using namespace std;
class Task {
public:
    string id, name, subject;
    int estimated_time, priority;
    time_t deadline;
    bool completed, active;
    /*هنا بنعرف الكلاس الاساسي
    فيها كل التاسك   
    id, name, subject, estimated_time, priority, deadline, completed / active */
    Task(string n, string sub, int studyTime, string dl, int pri = 5)
    /*ده الكونستركتور اللي بيستقبل بيانات المهمة ويخزنها، وكمان بيحدد 
    default priority
    لو المستخدم ما دخلش قيمة*/ 
        : name(n), subject(sub), estimated_time(studyTime), priority(pri), 
          completed(false), active(true) {
        id = generateID();
        /*كل مهمة بتاخد 
        ID 
        عشان نقدر نميزها بسهولة*/
        auto [parsedDeadline, valid] = Task::parseDate(dl);
        /*هنا بنحول التاريخ من 
        string
        إلي
        time
        ونأكد إنه صحيح*/
        if (valid) {
            deadline = parsedDeadline;
        } else {
            deadline = time(nullptr) + 7 * 24 * 3600;
            cerr << "\033[33m1 Invalid date, using 1 week from now\033[0m\n";
        }
    }
    Task() : estimated_time(0), priority(1), completed(false), active(false) {}
    double urgencyScore() const {
        /*دي دالة بتحسب مدى اهمية التاسك*/
        if (!active || completed) return -1.0;
        time_t now = time(nullptr);
        double daysLeft = difftime(deadline, now) / (24 * 60 * 60.0);
        /*بتحسب عدد الأيام المتبقية*/
        return max(0.0, (10 - daysLeft) * 0.5 + priority * 0.5);
        /*كل ما الوقت يقل او الاولوية تزيد -> المهمة تبقى اهم*/
    }
    bool isOverdue() const {
        return active && !completed && time(nullptr) > deadline;
    }
    string generateID() {
        auto now = chrono::high_resolution_clock::now().time_since_epoch().count();
        stringstream ss;
        ss << "T" << setw(13) << setfill('0') << now;
        return ss.str();
    }
    static pair<time_t, bool> parseDate(const string& dateStr) {
        smatch matches;
        if (regex_match(dateStr, matches, regex(R"(\d{4}-\d{2}-\d{2})"))) {
            struct tm tm = {};
            if (sscanf(dateStr.c_str(), "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) {
                tm.tm_year -= 1900; 
                tm.tm_mon -= 1; 
                tm.tm_hour = 23; tm.tm_min = 59; tm.tm_sec = 59;
                time_t result = mktime(&tm);
                if (result != -1) return {result, true};
            }
        }
        return {0, false};
    }
    string formatDate() const {
        char buf[20];
        struct tm* tm = localtime(&deadline);
        strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
        return string(buf);
    }
    string toString(int index = -1) const {
        string status;
        if (completed) status = "Done";
        else if (isOverdue()) status = "Overdue";
        else status = "Active";
        
        string idx = index > 0 ? to_string(index) + ". " : "";
        string shortId = id.substr(1, 8);
        return idx + status + " [" + shortId + "] " + 
               name.substr(0,20) + " | " + subject + 
               " | " + to_string(estimated_time) + "m | " + formatDate();
    }
};
//Priority Queue comparator
struct TaskCompare {
    bool operator()(Task* a, Task* b) {
        return a->urgencyScore() < b->urgencyScore();
    }
};
/*ده بيحدد ترتيب الاولويات داخل الـ 
priority queue
يعني الاعلى 
urgency
ييجي الاول*/
class TaskNode {
public:
    Task* task;
    TaskNode* next;
    TaskNode(Task* t) : task(t), next(nullptr) {}
};
/*كل 
Node
فيها
Task
و
pointer
على اللي بعدها 
ده -> linked list*/
class SmartStudyPlanner {
private:
    TaskNode* head;
    priority_queue<Task*, vector<Task*>, TaskCompare> pq;
    deque<Task*> executionQueue;
    /*عندنا 3 structures:
Linked List لتخزين كل المهام
Priority Queue لترتيبهم حسب الاهمية
Deque لعمل خطة يومية*/
    string filename;
    const int DAILY_LIMIT = 240;
    friend class WebServer;  // في الـ private section عشان يشوف head
    void rebuildPriorityQueue();
    void rebuildExecutionQueue();
    void saveData();
    void loadData();
    TaskNode* findNode(const string& taskId);
public:
    SmartStudyPlanner(string fn = "study_planner.json") : head(nullptr), filename(fn) {
        srand(time(nullptr));
        loadData();
        rebuildPriorityQueue();
        rebuildExecutionQueue();
    }
    ~SmartStudyPlanner() {
        saveData();
        deleteList(head);
    }
    vector<Task*> getActiveTasks() {
    vector<Task*> result;
    TaskNode* current = head;

    while (current) {
        if (current->task->active)
            result.push_back(current->task);
        current = current->next;
    }
    return result;
}
    void addTask();
    void updateTask();
    void toggleComplete();
    void deleteTask();
    void showAllTasks();
    void showPriorityTasks(int topN = 5);
    void searchTask();
    void checkOverdue();
    void dailyPlan();
    void addTaskFromWeb(string name, string subject, int time, string deadline, int priority) {
    Task* newTask = new Task(name, subject, time, deadline, priority);
    TaskNode* newNode = new TaskNode(newTask);
    
    newNode->next = head;
    head = newNode;

    rebuildPriorityQueue();
    rebuildExecutionQueue();
    saveData();
}
private:
    bool getIntInput(int& val, int min, int max, const string& prompt, bool required = true);
    bool getOptionalInt(int& val, int min, int max, const string& prompt);
    void deleteList(TaskNode* node);
};
void SmartStudyPlanner::deleteList(TaskNode* node) {
    while (node) {
        TaskNode* temp = node;
        node = node->next;
        delete temp->task;
        delete temp;
    }
}
void SmartStudyPlanner::rebuildPriorityQueue() {
    while (!pq.empty()) pq.pop();
    TaskNode* current = head;
    while (current) {
        if (current->task->active) {
            pq.push(current->task);
        }
        current = current->next;
    }
}
void SmartStudyPlanner::rebuildExecutionQueue() {
    executionQueue.clear();
    priority_queue<Task*, vector<Task*>, TaskCompare> temp = pq;
    int totalTime = 0; 
    while (!temp.empty() && totalTime < DAILY_LIMIT) {
        Task* task = temp.top();
        if (task->active && !task->completed) {
            executionQueue.push_back(task);
            totalTime += task->estimated_time;
        }
        temp.pop();
    }
}
TaskNode* SmartStudyPlanner::findNode(const string& taskId) {
    TaskNode* current = head;
    while (current) {
        if (current->task->id == taskId || 
            current->task->id.substr(1,8) == taskId) {
            return current;
        }
        current = current->next;
    }
    return nullptr;
}
bool SmartStudyPlanner::getIntInput(int& val, int min, int max, const string& prompt, bool required) {
    string input;
    cout << prompt;
    getline(cin >> ws, input);
    if (input.empty() && !required) return false;
    try {
        val = stoi(input);
        if (val < min || val > max) {
            cout << "Invalid range [" << min << "-" << max << "]" << endl;
            return false;
        }
        return true;
    } catch(...) {
        cout << "Enter a valid number!" << endl;
        return false;
    }
}
bool SmartStudyPlanner::getOptionalInt(int& val, int min, int max, const string& prompt) {
    return getIntInput(val, min, max, prompt, false);
}
void SmartStudyPlanner::addTask() {
    string name, subject, dlInput;
    int time, priority;
    cout << "Task name: "; getline(cin >> ws, name);
    cout << "Subject: "; getline(cin, subject);
    if (!getIntInput(time, 15, 480, "Estimated time (minutes) [15-480]: ")) return;
    cout << "Deadline (YYYY-MM-DD): "; getline(cin, dlInput);
    auto [parsedDeadline, valid] = Task::parseDate(dlInput);
    if (!valid) {
        cout << "Invalid date format! Use YYYY-MM-DD" << endl;
        return;
    }
    if (!getIntInput(priority, 1, 10, "Priority [1-10]: ")) return;
    Task* newTask = new Task(name, subject, time, dlInput, priority);
    /*هنا بننشئ 
    task جديدة*/
    TaskNode* newNode = new TaskNode(newTask);
    newNode->next = head;
    head = newNode;
    /*بنضيفها في بداية الـ linked list*/
    rebuildPriorityQueue();
    rebuildExecutionQueue();
    saveData();
    cout << "Added successfully:" << endl;
    cout << newTask->toString() << endl;
}
void SmartStudyPlanner::updateTask() {
    showAllTasks();
    string taskId;
    cout << "ID (8 digits or full): "; getline(cin >> ws, taskId);
    TaskNode* node = findNode(taskId);
    if (!node) {
        cout << "Task not found!" << endl;
        return;
    }
    Task* task = node->task;
    cout << "Editing: " << task->name << endl;
    string newName, newSubject, newDeadline;
    int newTime = -1, newPriority = -1;
    cout << "New name (Enter=skip): "; getline(cin, newName);
    if (!newName.empty()) task->name = newName;
    cout << "New subject (Enter=skip): "; getline(cin, newSubject);
    if (!newSubject.empty()) task->subject = newSubject;
    getOptionalInt(newTime, 15, 480, "New time: ");
    if (newTime != -1) task->estimated_time = newTime;
    cout << "New deadline (Enter=skip): "; getline(cin, newDeadline);
    if (!newDeadline.empty()) {
        auto [parsed, valid] = Task::parseDate(newDeadline);
        if (valid) task->deadline = parsed;
    }
    getOptionalInt(newPriority, 1, 10, "New priority: ");
    if (newPriority != -1) task->priority = newPriority;
    rebuildPriorityQueue();
    /*بنعيد ترتيب الأولويات بعد الإضافة*/
    rebuildExecutionQueue();
    saveData();
    cout << "Task updated!" << endl;
}
void SmartStudyPlanner::toggleComplete() {
    showAllTasks();
    string taskId;
    cout << "ID to mark complete: "; getline(cin >> ws, taskId);
    TaskNode* node = findNode(taskId);
    if (!node || node->task->completed) {
        cout << "Not found or already completed!" << endl;
        return;
    }
    node->task->completed = true;
    /*بنعلم المهمة إنها خلصت*/
    rebuildPriorityQueue();
    rebuildExecutionQueue();
    saveData();
    cout << node->task->name << " marked as done!" << endl;
}
void SmartStudyPlanner::deleteTask() {
    showAllTasks();
    string taskId;
    cout << "ID to delete: "; getline(cin >> ws, taskId);
    TaskNode* node = findNode(taskId);
    if (!node) {
        cout << "Not found!" << endl;
        return;
    }
    node->task->active = false;
    /*بنلغي تفعيل المهمة بدل ما نمسحها من الذاكرة*/
    rebuildPriorityQueue();
    rebuildExecutionQueue();
    saveData();
    cout << node->task->name << "deleted" << endl;
}
void SmartStudyPlanner::showAllTasks() {
    cout << "\nAll Tasks:" << endl;
    cout << string(90, '-') << endl;
    TaskNode* current = head;
    int i = 1;
    if (!current) {
        cout << "No tasks!" << endl;
        return;
    }
    while (current) {
        if (current->task->active) {
            cout << current->task->toString(i) << endl;
            i++;
        }
        current = current->next;
    }
}
void SmartStudyPlanner::dailyPlan() {
    cout << "\nSmart Daily Plan (Max: " << DAILY_LIMIT << " minutes):" << endl;
    cout << string(70, '-') << endl;
    int totalTime = 0;
    size_t i = 0;
    while (i < executionQueue.size() && totalTime < DAILY_LIMIT) {
        /*بنختار مهام لحد 240 دقيقة في اليوم*/
        Task* task = executionQueue[i];
        if (task->active && !task->completed) {
            cout << task->toString(i+1) << endl;
            totalTime += task->estimated_time;
        }
        i++;
    }
    cout << "\nTotal: " << totalTime << "/" << DAILY_LIMIT << " minutes" << endl;
}
void SmartStudyPlanner::checkOverdue() {
    cout << "\nOverdue Tasks:" << endl;
    TaskNode* current = head;
    int count = 0;
    while (current) {
        if (current->task->isOverdue()) {
            cout << current->task->toString() << endl;
            count++;
        }
        current = current->next;
    }
    if (!count) cout << "Perfect! No overdue tasks" << endl;
}
void SmartStudyPlanner::searchTask() {
    string keyword;
    cout << "Search: "; getline(cin >> ws, keyword);
    TaskNode* current = head;
    bool found = false;
    while (current) {
        if (current->task->active && 
            (current->task->name.find(keyword) /*بندور على أي task فيها كلمة معينة*/!= string::npos ||
             current->task->subject.find(keyword) /*بندور على أي task فيها كلمة معينة*/!= string::npos)) {
            cout << current->task->toString() << endl;
            found = true;
        }
        current = current->next;
    }
    if (!found) cout << "No results" << endl;
}
void SmartStudyPlanner::saveData() /*بنخزن البيانات في ملف JSON-like عشان ما تضيعش بعد تشغيل البرنامج*/ {
    ofstream file(filename);
    file << "[\n";
    TaskNode* current = head;
    bool first = true;
    while (current) {
        if (current->task->active) {
            if (!first) file << ",\n";
            file << "  {\"id\":\"" << current->task->id 
                 << "\",\"name\":\"" << current->task->name 
                 << "\",\"subject\":\"" << current->task->subject
                 << "\",\"time\":" << current->task->estimated_time
                 << ",\"deadline\":\"" << current->task->formatDate()
                 << "\",\"priority\":" << current->task->priority
                 << ",\"completed\":" << (current->task->completed ? "true" : "false")
                 << ",\"active\":true}";
            first = false;
        }
        current = current->next;
    }
    file << "\n]";
}
void SmartStudyPlanner::loadData() {
    ifstream file(filename);
    if (!file.is_open()) return;
    cout << "Data loaded from " << filename << endl;
}

class WebServer {
public:
    WebServer() {
        initSockets();
    }

    ~WebServer() {
        cleanupSockets();
    }

    void start(SmartStudyPlanner& planner) {
        socket_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == INVALID_SOCK) {
            cerr << "Failed to create socket\n";
            return;
        }

        // Allow quick restarts while developing
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&opt), sizeof(opt));

        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(3000);

        if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
            cerr << "Bind failed on port 3000\n";
            closeSocket(server_fd);
            return;
        }
        listen(server_fd, SOMAXCONN);
        cout << "Server OK -> http://localhost:3000\n";
        cout.flush();

        while (true) {
            socket_t client_fd = accept(server_fd, NULL, NULL);
            if (client_fd != INVALID_SOCK) {
                thread([this, client_fd, &planner]() {
                    handleRequest(client_fd, planner);
                    closeSocket(client_fd);
                }).detach();
            }
        }
        closeSocket(server_fd);
    }

private:
    /** Append a line to server.log — console output is unreliable when redirected. */
    static void logDebug(const std::string& message) {
        std::ofstream log("server.log", std::ios::app);
        if (log) log << message << "\n";
    }

    void handleRequest(socket_t client_fd, SmartStudyPlanner& planner) {
        char buffer[16384] = {0};
        string request;

        // Read until the full request (headers + body) has arrived.
        // A single recv() is not enough: headers and body can come in
        // separate TCP segments, and the Content-Length tells us how
        // many body bytes to expect.
        size_t expectedTotal = 0;
        while (true) {
            int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) break;
            buffer[bytes] = '\0';
            request.append(buffer, bytes);

            if (expectedTotal == 0) {
                auto headerEnd = request.find("\r\n\r\n");
                if (headerEnd != string::npos) {
                    size_t contentLength = 0;
                    auto clPos = request.find("Content-Length:");
                    if (clPos == string::npos) clPos = request.find("content-length:");
                    if (clPos != string::npos) {
                        auto lineEnd = request.find("\r\n", clPos);
                        string value = request.substr(clPos + 15, lineEnd - clPos - 15);
                        contentLength = (size_t)atoll(value.c_str());
                    }
                    expectedTotal = headerEnd + 4 + contentLength;
                }
            }

            if (expectedTotal > 0 && request.size() >= expectedTotal) break;

            // No body expected (GET) — headers end with CRLFCRLF or LFLF
            if (expectedTotal == 0 &&
                (request.find("\r\n\r\n") != string::npos || request.find("\n\n") != string::npos)) break;
        }

        if (request.empty()) return;
        cout << "[REQ] " << request.substr(0, 100) << "...\n";

        // --- parse request line: "METHOD PATH HTTP/1.1" ---
        string method, path;
        {
            istringstream line(request);
            string version;
            line >> method >> path >> version;
        }

        // strip query string
        auto q = path.find('?');
        if (q != string::npos) path = path.substr(0, q);

        // file-based debug log (console output is unreliable under redirection)
        {
            ofstream log("server.log", ios::app);
            log << "[" << time(nullptr) << "] " << method << " " << path
                << " (" << request.size() << " bytes)\n";
            log << "--- RAW ---\n" << request << "\n--- END ---\n";
        }

        // API: GET /api/tasks  -> JSON list
        if (method == "GET" && path == "/api/tasks") {
            sendTasksJSON(client_fd, planner);
        }
        // API: POST /api/tasks -> add task, then return the list
        else if (method == "POST" && path == "/api/tasks") {
            parsePOSTData(request, planner);
            sendTasksJSON(client_fd, planner);
        }
        // Health endpoint (used by CI smoke test)
        else if (method == "GET" && path == "/health") {
            string body = "{\"status\":\"ok\"}";
            string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                          "Access-Control-Allow-Origin: *\r\nContent-Length: " +
                          to_string(body.size()) + "\r\n\r\n" + body;
            send(client_fd, resp.c_str(), (int)resp.size(), 0);
        }
        // Root page
        else if (method == "GET" && (path == "/" || path == "/index.html")) {
            sendHTML(client_fd);
        }
        // 404
        else {
            string html = "<h1 style='color:red'>404 Not Found</h1>";
            string resp = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=utf-8\r\n\r\n" + html;
            send(client_fd, resp.c_str(), (int)resp.size(), 0);
        }
    }

    void sendHTML(socket_t client_fd) {
        string html = R"HTML(
<!DOCTYPE html>

<html lang="ar" dir="rtl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>📚 Smart Study Planner</title>
<link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.2/dist/css/bootstrap.min.css" rel="stylesheet">
<link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css" rel="stylesheet">
<style>
html{direction:rtl;overflow-x:hidden;}
body{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;padding:20px;margin:0;overflow-x:hidden;direction:rtl;max-width:100%;}
.card{background:rgba(255,255,255,0.95);backdrop-filter:blur(10px);border-radius:20px;border:none;box-shadow:0 20px 40px rgba(0,0,0,0.1);}
.btn{border-radius:25px;font-weight:600;transition:all 0.3s;}
.btn:hover{transform:translateY(-2px);box-shadow:0 10px 20px rgba(0,0,0,0.2);}
.table-hover tbody tr:hover{background:rgba(102,126,234,0.1);}
</style>
</head>
<body>
<div class="container mt-5">
<h1 class="text-center text-white mb-4"><i class="fas fa-graduation-cap"></i> Smart Study Planner v3.0</h1>

<!-- Add Form -->
<div class="card mb-4 p-4">
<h5 class="mb-3"><i class="fas fa-plus text-success"></i> إضافة مهمة</h5>
<form id="taskForm" class="row g-3">
<div class="col-md-3"><input type="text" class="form-control" id="name" placeholder="اسم المهمة" required></div>
<div class="col-md-2"><input type="text" class="form-control" id="subject" placeholder="المادة" required></div>
<div class="col-md-2"><input type="number" class="form-control" id="time" min="15" max="480" value="60" required></div>
<div class="col-md-2"><input type="date" class="form-control" id="date" required></div>
<div class="col-md-1"><input type="number" class="form-control" id="pri" min="1" max="10" value="5"></div>
<div class="col-md-2"><button type="submit" class="btn btn-success w-100">إضافة</button></div>
</form>
</div>

<!-- Tasks -->
<div class="card p-4">
<h5 class="mb-3"><i class="fas fa-list"></i> المهام <button class="btn btn-sm btn-info ms-2" onclick="refresh()">🔄</button></h5>
<table class="table table-hover" id="tasks">
<thead class="table-dark"><tr><th>حالة</th><th>ID</th><th>مهمة</th><th>مادة</th><th>وقت</th><th>موعد</th></tr></thead>
<tbody id="taskBody"><tr><td colspan="6" class="text-center p-4">جاري التحميل...</td></tr></tbody>
</table>
</div>
</div>

<script>
let tasks = [];
async function refresh() {
    try {
        const res = await fetch('/api/tasks');
        tasks = await res.json();
        document.getElementById('taskBody').innerHTML = tasks.map(t=>`
            <tr>
                <td><span class="badge bg-${t.status==='Done'?'success':t.status==='Overdue'?'danger':'warning'}">${t.status}</span></td>
                <td><code>${t.shortId}</code></td>
                <td>${t.name}</td>
                <td>${t.subject}</td>
                <td>${t.estimated_time}m</td>
                <td>${t.deadline}</td>
            </tr>`).join('') || '<tr><td colspan="6" class="text-center p-4">لا توجد مهام</td></tr>';
    } catch(e) {console.error(e);}
}

document.getElementById('taskForm').onsubmit = async(e)=>{
    e.preventDefault();
    const data = {
        name: document.getElementById('name').value,
        subject: document.getElementById('subject').value,
        estimated_time: parseInt(document.getElementById('time').value),
        deadline: document.getElementById('date').value,
        priority: parseInt(document.getElementById('pri').value)
    };
    try {
        await fetch('/api/tasks', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(data)});
        e.target.reset();
        document.getElementById('date').valueAsDate = new Date(Date.now()+7*86400000);
        refresh();
    } catch(e) {alert('خطأ!');}
};

refresh();
setInterval(refresh, 3000);
</script>
</body></html>)HTML";

        string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html; charset=utf-8\r\n";
        response += "Cache-Control: no-cache\r\n\r\n";
        response += html;
        send(client_fd, response.c_str(), (int)response.length(), 0);
    }

    void parsePOSTData(const string& request, SmartStudyPlanner& planner) {
        // Find the header/body separator. Different clients use either
        // CRLFCRLF (standard) or LFLF (lenient). Supporting both makes the
        // server work with browsers, curl and PowerShell alike.
        size_t bodyStart = request.find("\r\n\r\n");
        size_t sepLen = 4;

        if (bodyStart == string::npos) {
            bodyStart = request.find("\n\n");
            sepLen = 2;
        }
        if (bodyStart == string::npos) {
            logDebug("  parsePOSTData: no header separator found");
            return;
        }

        string body = request.substr(bodyStart + sepLen);
        logDebug("  body: " + body.substr(0, 150));

        try {
            string name, subject, dl;
            int time = 60, pri = 5;

            // Debug: dump the exact positions so parsing errors are visible
            logDebug("  body length: " + to_string(body.size()));

            // Parse name
            size_t pos = body.find("\"name\":");
            if (pos != string::npos) {
                pos = body.find('"', pos + 7) + 1;        // skip the opening quote
                size_t end = body.find('"', pos);
                if (end != string::npos) name = body.substr(pos, end - pos);
            }
            logDebug("  name='" + name + "'");

            // Parse subject
            pos = body.find("\"subject\":");
            if (pos != string::npos) {
                pos = body.find('"', pos + 10) + 1;
                size_t end = body.find('"', pos);
                if (end != string::npos) subject = body.substr(pos, end - pos);
            }
            logDebug("  subject='" + subject + "'");

            // Parse estimated_time (number)
            pos = body.find("\"estimated_time\":");
            if (pos != string::npos) {
                size_t valueStart = pos + 17;             // len("\"estimated_time\":") == 17
                size_t end = body.find_first_of(",}", valueStart);
                if (end != string::npos) {
                    string raw = body.substr(valueStart, end - valueStart);
                    time = atoi(raw.c_str());
                }
            }
            logDebug("  time=" + to_string(time));

            // Parse deadline
            pos = body.find("\"deadline\":");
            if (pos != string::npos) {
                pos = body.find('"', pos + 11) + 1;
                size_t end = body.find('"', pos);
                if (end != string::npos) dl = body.substr(pos, end - pos);
            }
            logDebug("  deadline='" + dl + "'");

            // Parse priority (number)
            pos = body.find("\"priority\":");
            if (pos != string::npos) {
                size_t valueStart = pos + 11;             // len("\"priority\":") == 11
                size_t end = body.find_first_of(",}", valueStart);
                if (end != string::npos) {
                    string raw = body.substr(valueStart, end - valueStart);
                    pri = atoi(raw.c_str());
                }
            }
            logDebug("  priority=" + to_string(pri));

            if (!name.empty() && !subject.empty()) {
                logDebug("  calling addTaskFromWeb");
                planner.addTaskFromWeb(name, subject, time, dl, pri);
                logDebug("  addTaskFromWeb done");
            } else {
                logDebug("  WARN missing fields");
            }
        } catch (const std::exception& ex) {
            logDebug(string("  ERR parsePOSTData: ") + ex.what());
        }
    }

    void sendTasksJSON(socket_t client_fd, SmartStudyPlanner& planner) {
        vector<Task*> tasks = planner.getActiveTasks();
        string json = "[";

        for (size_t i = 0; i < tasks.size(); ++i) {
            Task* t = tasks[i];
            string status = t->completed ? "Done" : t->isOverdue() ? "Overdue" : "Active";
            string shortId = t->id.substr(1, 8);

            if (i > 0) json += ",";
            json += "{";
            json += "\"name\":\"" + escapeJson(t->name) + "\",";
            json += "\"subject\":\"" + escapeJson(t->subject) + "\",";
            json += "\"estimated_time\":" + to_string(t->estimated_time) + ",";
            json += "\"deadline\":\"" + t->formatDate() + "\",";
            json += "\"priority\":" + to_string(t->priority) + ",";
            json += "\"status\":\"" + status + "\",";
            json += "\"shortId\":\"" + shortId + "\"";
            json += "}";
        }
        json += "]";

        string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json; charset=utf-8\r\n";
        response += "Access-Control-Allow-Origin: *\r\n\r\n";
        response += json;

        send(client_fd, response.c_str(), (int)response.length(), 0);
        cout << "📤 Sent " << tasks.size() << " tasks\n";
    }

    string escapeJson(const string& s) {
        string result = s;
        replaceAll(result, "\\", "\\\\");
        replaceAll(result, "\"", "\\\"");
        replaceAll(result, "\n", "\\n");
        replaceAll(result, "\r", "\\r");
        replaceAll(result, "\t", "\\t");
        return result;
    }

    void replaceAll(string& str, const string& from, const string& to) {
        size_t start = 0;
        while ((start = str.find(from, start)) != string::npos) {
            str.replace(start, from.length(), to);
            start += to.length();
        }
    }
};

int main(int argc, char** argv) {
    // --selftest: exercise the planner without starting the network server.
    // Used by CI so the build can be validated headlessly.
    if (argc > 1 && string(argv[1]) == "--selftest") {
        cout << "[selftest] starting\n";

        SmartStudyPlanner planner("selftest.json");
        planner.addTaskFromWeb("Sample Task", "Testing", 60, "2026-12-31", 5);

        auto tasks = planner.getActiveTasks();
        cout << "[selftest] tasks=" << tasks.size() << "\n";
        if (!tasks.empty()) {
            cout << "[selftest] first=" << tasks.front()->name
                 << " subject=" << tasks.front()->subject
                 << " priority=" << tasks.front()->priority << "\n";
        }

        bool ok = tasks.size() == 1 && tasks.front()->name == "Sample Task";
        cout << "[selftest] " << (ok ? "PASS" : "FAIL") << "\n";
        return ok ? 0 : 1;
    }

    SmartStudyPlanner planner;
    WebServer server;
    server.start(planner);
    return 0;
}