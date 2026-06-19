#====================================================================================================
# START - Testing Protocol - DO NOT EDIT OR REMOVE THIS SECTION
#====================================================================================================

# THIS SECTION CONTAINS CRITICAL TESTING INSTRUCTIONS FOR BOTH AGENTS
# BOTH MAIN_AGENT AND TESTING_AGENT MUST PRESERVE THIS ENTIRE BLOCK

# Communication Protocol:
# If the `testing_agent` is available, main agent should delegate all testing tasks to it.
#
# You have access to a file called `test_result.md`. This file contains the complete testing state
# and history, and is the primary means of communication between main and the testing agent.
#
# Main and testing agents must follow this exact format to maintain testing data. 
# The testing data must be entered in yaml format Below is the data structure:
# 
## user_problem_statement: {problem_statement}
## backend:
##   - task: "Task name"
##     implemented: true
##     working: true  # or false or "NA"
##     file: "file_path.py"
##     stuck_count: 0
##     priority: "high"  # or "medium" or "low"
##     needs_retesting: false
##     status_history:
##         -working: true  # or false or "NA"
##         -agent: "main"  # or "testing" or "user"
##         -comment: "Detailed comment about status"
##
## frontend:
##   - task: "Task name"
##     implemented: true
##     working: true  # or false or "NA"
##     file: "file_path.js"
##     stuck_count: 0
##     priority: "high"  # or "medium" or "low"
##     needs_retesting: false
##     status_history:
##         -working: true  # or false or "NA"
##         -agent: "main"  # or "testing" or "user"
##         -comment: "Detailed comment about status"
##
## metadata:
##   created_by: "main_agent"
##   version: "1.0"
##   test_sequence: 0
##   run_ui: false
##
## test_plan:
##   current_focus:
##     - "Task name 1"
##     - "Task name 2"
##   stuck_tasks:
##     - "Task name with persistent issues"
##   test_all: false
##   test_priority: "high_first"  # or "sequential" or "stuck_first"
##
## agent_communication:
##     -agent: "main"  # or "testing" or "user"
##     -message: "Communication message between agents"

# Protocol Guidelines for Main agent
#
# 1. Update Test Result File Before Testing:
#    - Main agent must always update the `test_result.md` file before calling the testing agent
#    - Add implementation details to the status_history
#    - Set `needs_retesting` to true for tasks that need testing
#    - Update the `test_plan` section to guide testing priorities
#    - Add a message to `agent_communication` explaining what you've done
#
# 2. Incorporate User Feedback:
#    - When a user provides feedback that something is or isn't working, add this information to the relevant task's status_history
#    - Update the working status based on user feedback
#    - If a user reports an issue with a task that was marked as working, increment the stuck_count
#    - Whenever user reports issue in the app, if we have testing agent and task_result.md file so find the appropriate task for that and append in status_history of that task to contain the user concern and problem as well 
#
# 3. Track Stuck Tasks:
#    - Monitor which tasks have high stuck_count values or where you are fixing same issue again and again, analyze that when you read task_result.md
#    - For persistent issues, use websearch tool to find solutions
#    - Pay special attention to tasks in the stuck_tasks list
#    - When you fix an issue with a stuck task, don't reset the stuck_count until the testing agent confirms it's working
#
# 4. Provide Context to Testing Agent:
#    - When calling the testing agent, provide clear instructions about:
#      - Which tasks need testing (reference the test_plan)
#      - Any authentication details or configuration needed
#      - Specific test scenarios to focus on
#      - Any known issues or edge cases to verify
#
# 5. Call the testing agent with specific instructions referring to test_result.md
#
# IMPORTANT: Main agent must ALWAYS update test_result.md BEFORE calling the testing agent, as it relies on this file to understand what to test next.

#====================================================================================================
# END - Testing Protocol - DO NOT EDIT OR REMOVE THIS SECTION
#====================================================================================================



#====================================================================================================
# Testing Data - Main Agent and testing sub agent both should log testing data below this section
#====================================================================================================
user_problem_statement: "Debug the kegstill app from errors - backend focus"

backend:
  - task: "Server startup - environment variables"
    implemented: true
    working: true
    file: "backend/server.py"
    stuck_count: 0
    priority: "high"
    needs_retesting: false
    status_history:
        - working: false
          agent: "testing"
          comment: "CRITICAL BUG FOUND: server.py used os.environ['MONGO_URL'] and os.environ['DB_NAME'] which crash with KeyError if env vars are missing and no .env file exists. Fixed to use os.environ.get() with sensible defaults (mongodb://localhost:27017 and 'kegstill'). Also created backend/.env file with proper values."
        - working: true
          agent: "testing"
          comment: "After fix, server starts cleanly with or without explicit env vars. GET /api/ returns 200 OK."

  - task: "Deprecated on_event handler"
    implemented: true
    working: true
    file: "backend/server.py"
    stuck_count: 0
    priority: "medium"
    needs_retesting: false
    status_history:
        - working: false
          agent: "testing"
          comment: "FastAPI 0.110.1 emits DeprecationWarning for @app.on_event('shutdown'). Replaced with asynccontextmanager lifespan pattern per FastAPI docs."
        - working: true
          agent: "testing"
          comment: "After rewrite, zero deprecation warnings. Lifespan handler properly closes MongoDB connection on shutdown."

  - task: "POST /api/status endpoint"
    implemented: true
    working: true
    file: "backend/server.py"
    stuck_count: 0
    priority: "high"
    needs_retesting: false
    status_history:
        - working: true
          agent: "testing"
          comment: "Creates status check in MongoDB, returns proper JSON with id, client_name, timestamp. Validates input correctly (returns 422 for missing fields or invalid JSON)."

  - task: "GET /api/status endpoint"
    implemented: true
    working: true
    file: "backend/server.py"
    stuck_count: 0
    priority: "high"
    needs_retesting: false
    status_history:
        - working: true
          agent: "testing"
          comment: "Returns list of all status checks from MongoDB. Properly handles ISO string to datetime conversion. Excludes MongoDB _id field."

  - task: "CORS middleware"
    implemented: true
    working: true
    file: "backend/server.py"
    stuck_count: 0
    priority: "medium"
    needs_retesting: false
    status_history:
        - working: true
          agent: "testing"
          comment: "CORS configured to allow all origins by default (CORS_ORIGINS env var, defaults to '*'). Frontend should be able to reach backend cross-origin."

metadata:
  created_by: "testing_agent"
  version: "1.0"
  test_sequence: 1
  run_ui: false

test_plan:
  current_focus:
    - "Server startup - environment variables"
    - "Deprecated on_event handler"
  stuck_tasks: []
  test_all: false
  test_priority: "high_first"

agent_communication:
    - agent: "testing"
      message: |
        BACKEND DEBUG SUMMARY:
        
        BUGS FOUND AND FIXED:
        1. CRITICAL: Missing .env file + hard crash on missing MONGO_URL/DB_NAME env vars.
           - server.py line 18-20 used os.environ['MONGO_URL'] which throws KeyError.
           - FIX: Changed to os.environ.get() with defaults + created .env file.
        
        2. DEPRECATION: @app.on_event("shutdown") is deprecated in FastAPI 0.110.1.
           - FIX: Replaced with asynccontextmanager lifespan pattern.
        
        3. MINOR: Logging was configured AFTER app creation (cosmetic, but startup logs missed).
           - FIX: Moved logging config before app instantiation.
        
        ALL ENDPOINTS TESTED AND WORKING:
        - GET /api/ -> 200 {"message": "Hello World"}
        - POST /api/status -> 200 (creates record)
        - GET /api/status -> 200 (returns all records)
        - Invalid input -> 422 (proper validation)
        - Unknown routes -> 404
        
        NOTE: The actual kegstill app logic lives in ESP32 Arduino firmware
        (kegstill_mvp/kegstill_mvp.ino). The FastAPI backend is just a scaffold/
        boilerplate. The real "app" is the embedded firmware + its built-in web server.
        If the user's "errors" are in the Arduino code, that cannot be tested here
        (requires physical hardware). The frontend (React) connects to this backend.
