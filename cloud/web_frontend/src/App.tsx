import { BrowserRouter, Routes, Route, Navigate } from "react-router-dom";
import { Layout } from "./components/Layout";
import { ThemeProvider } from "./components/ThemeProvider";
import { ForbiddenModal } from "./components/ForbiddenModal";
import { ProtectedRoute } from "./components/ProtectedRoute";
import { AuthProvider } from "./contexts/AuthContext";
import { Dashboard } from "./pages/Dashboard";
import { BarcodeList } from "./pages/BarcodeList";
import { BarcodeCreate } from "./pages/BarcodeCreate";
import { CategoryList } from "./pages/CategoryList";
import { CategoryCreate } from "./pages/CategoryCreate";
import { Login } from "./pages/Login";
import { Register } from "./pages/Register";
import { UserList } from "./pages/UserList";
import { Videos } from "./pages/Videos";
import { ManualControl } from "./pages/ManualControl";

function App() {
  return (
    <ThemeProvider>
      <BrowserRouter>
        <AuthProvider>
          <ForbiddenModal />
          <Routes>
            <Route path="/login" element={<Login />} />
            <Route path="/register" element={<Register />} />
            <Route
              path="/"
              element={
                <ProtectedRoute>
                  <Layout />
                </ProtectedRoute>
              }
            >
              <Route index element={<Dashboard />} />
              <Route path="barcodes" element={<BarcodeList />} />
              <Route path="barcodes/new" element={<BarcodeCreate />} />
              <Route path="categories" element={<CategoryList />} />
              <Route path="categories/new" element={<CategoryCreate />} />
              <Route path="users" element={<UserList />} />
              <Route path="videos" element={<Videos />} />
              <Route path="manual-control" element={<ManualControl />} />

            </Route>
            <Route path="*" element={<Navigate to="/" replace />} />
          </Routes>
        </AuthProvider>
      </BrowserRouter>
    </ThemeProvider>
  );
}

export default App;
