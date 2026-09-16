import axios from "axios";
import type {
  Barcode,
  Category,
  PaginatedResponse,
  PaginationQuery,
  LoginInput,
  RegisterInput,
  SendCodeInput,
  LoginResponse,
  User,
  CreateUserInput,
  UpdateUserInput,
  GroupCountRow,
  Video,
} from "../types";

const api = axios.create({
  baseURL: import.meta.env.VITE_API_BASE_URL || "/api",
  timeout: 10000,
});

api.interceptors.request.use((config) => {
  const token = localStorage.getItem("token");
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

api.interceptors.response.use(
  (response) => response,
  (error) => {
    if (error.response?.status === 401) {
      localStorage.removeItem("token");
      localStorage.removeItem("user");
      if (window.location.pathname !== "/login") {
        window.location.href = "/login";
      }
    }
    if (error.response?.status === 403) {
      const msg = error.response.data?.error || "无权限执行此操作";
      window.dispatchEvent(new CustomEvent("forbidden", { detail: msg }));
    }
    return Promise.reject(error);
  },
);

export const authApi = {
  login: async (input: LoginInput) => {
    const { data } = await api.post<LoginResponse>("/auth/login", input);
    return data;
  },
  register: async (input: RegisterInput) => {
    const { data } = await api.post<User>("/auth/register", input);
    return data;
  },
  sendCode: async (input: SendCodeInput) => {
    const { data } = await api.post<{ message: string }>(
      "/auth/send-code",
      input,
    );
    return data;
  },
};

export const categoryApi = {
  getList: async (query?: PaginationQuery) => {
    const params = new URLSearchParams();
    if (query?.page) params.append("page", String(query.page));
    if (query?.page_size) params.append("page_size", String(query.page_size));
    const { data } = await api.get<PaginatedResponse<Category>>(
      `/categories?${params}`,
    );
    return data;
  },
  getAll: async () => {
    const { data } = await api.get<Category[]>("/categories/all");
    return data;
  },
  getById: async (id: number) => {
    const { data } = await api.get<Category>(`/categories/${id}`);
    return data;
  },
  create: async (input: {
    name: string;
    field_defs: { key: string; label: string }[];
  }) => {
    const { data } = await api.post<Category>("/categories", input);
    return data;
  },
  update: async (
    id: number,
    input: { name: string; field_defs: { key: string; label: string }[] },
  ) => {
    const { data } = await api.put<Category>(`/categories/${id}`, input);
    return data;
  },
  delete: async (id: number) => {
    const { data } = await api.delete(`/categories/${id}`);
    return data;
  },
};

export const barcodeApi = {
  getList: async (params: {
    category_id: number;
    page?: number;
    page_size?: number;
    [key: string]: string | number | undefined;
  }) => {
    const sp = new URLSearchParams();
    sp.append("category_id", String(params.category_id));
    if (params.page) sp.append("page", String(params.page));
    if (params.page_size) sp.append("page_size", String(params.page_size));
    for (const [key, value] of Object.entries(params)) {
      if (!["category_id", "page", "page_size"].includes(key) && value) {
        sp.append(key, String(value));
      }
    }
    const { data } = await api.get<PaginatedResponse<Barcode>>(
      `/barcodes?${sp}`,
    );
    return data;
  },

  getById: async (id: number) => {
    const { data } = await api.get<Barcode>(`/barcodes/${id}`);
    return data;
  },

  create: async (formData: FormData) => {
    const { data } = await api.post<Barcode>("/barcodes", formData, {
      headers: { "Content-Type": "multipart/form-data" },
    });
    return data;
  },

  update: async (id: number, formData: FormData) => {
    const { data } = await api.put<Barcode>(`/barcodes/${id}`, formData, {
      headers: { "Content-Type": "multipart/form-data" },
    });
    return data;
  },

  delete: async (id: number) => {
    const { data } = await api.delete(`/barcodes/${id}`);
    return data;
  },

  statsByCategory: async () => {
    const { data } = await api.get<GroupCountRow[]>(
      "/barcodes/stats/by-category",
    );
    return data;
  },
};

export const videoApi = {
  getList: async (params: { page?: number; page_size?: number }) => {
    const sp = new URLSearchParams();
    if (params.page) sp.append("page", String(params.page));
    if (params.page_size) sp.append("page_size", String(params.page_size));
    const { data } = await api.get<PaginatedResponse<Video>>(
      `/videos?${sp}`,
    );
    return data;
  },

  delete: async (id: number) => {
    const { data } = await api.delete(`/videos/${id}`);
    return data;
  },
};

export const userApi = {
  getList: async () => {
    const { data } = await api.get<User[]>("/users");
    return data;
  },
  create: async (input: CreateUserInput) => {
    const { data } = await api.post<User>("/users", input);
    return data;
  },
  update: async (id: string, input: UpdateUserInput) => {
    const { data } = await api.put<User>(`/users/${id}`, input);
    return data;
  },
  delete: async (id: string) => {
    const { data } = await api.delete(`/users/${id}`);
    return data;
  },
};
